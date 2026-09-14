#include "LibraryEnrichmentService.h"

#include "AppController.h"
#include "ConfigService.h"
#include "DiscogsService.h"
#include "LibraryService.h"
#include "LyricsService.h"
#include "MetadataSearchService.h"
#include "TagService.h"

#include <QFile>
#include <QFileInfo>
#include <QTimer>

namespace {

constexpr int kDiscogsStepDelayMs = 2000;

} // namespace

LibraryEnrichmentService::LibraryEnrichmentService(ConfigService *config, LibraryService *library,
                                                   DiscogsService *discogs,
                                                   MetadataSearchService *metadataSearch,
                                                   TagService *tags, LyricsService *lyrics,
                                                   AppController *app, QObject *parent)
    : QObject(parent)
    , m_config(config)
    , m_library(library)
    , m_discogs(discogs)
    , m_metadataSearch(metadataSearch)
    , m_tags(tags)
    , m_lyrics(lyrics)
    , m_app(app)
{
    connect(m_discogs, &DiscogsService::artistsSearchFinished, this,
            &LibraryEnrichmentService::onArtistsSearchFinished);
    connect(m_discogs, &DiscogsService::artistFetched, this,
            &LibraryEnrichmentService::onArtistFetched);
    connect(m_discogs, &DiscogsService::releaseFetched, this,
            &LibraryEnrichmentService::onReleaseFetched);
    connect(m_metadataSearch, &MetadataSearchService::searchFinished, this,
            &LibraryEnrichmentService::onTitleFixSearchFinished);
    connect(m_metadataSearch, &MetadataSearchService::candidatesChanged, this, [this]() {
        if (m_active && m_wait == WaitKind::TitleFixDetail) {
            onTitleFixDetailMaybeReady();
        } else if (m_active && m_paused && m_resolveKind == QStringLiteral("titleFix")) {
            mirrorTitleFixCandidates();
            emitState();
        }
    });
    connect(m_metadataSearch, &MetadataSearchService::titleFixProposalsChanged, this, [this]() {
        if (m_active && m_wait == WaitKind::TitleFixDetail) {
            onTitleFixDetailMaybeReady();
        } else if (m_active && m_paused && m_resolveKind == QStringLiteral("titleFix")) {
            emitState();
        }
    });
    connect(m_metadataSearch, &MetadataSearchService::statusChanged, this, [this]() {
        if (m_active && m_wait == WaitKind::TitleFixDetail) {
            onTitleFixDetailMaybeReady();
        }
    });
    connect(m_lyrics, &LyricsService::busyChanged, this,
            &LibraryEnrichmentService::onLyricsBusyChanged);
    connect(m_lyrics, &LyricsService::progressChanged, this,
            &LibraryEnrichmentService::onLyricsProgressChanged);
}

QVariantList LibraryEnrichmentService::titleFixProposals() const
{
    return m_metadataSearch ? m_metadataSearch->titleFixProposals() : QVariantList{};
}

QVariantList LibraryEnrichmentService::titleFixUnmatched() const
{
    return m_metadataSearch ? m_metadataSearch->titleFixUnmatched() : QVariantList{};
}

bool LibraryEnrichmentService::start()
{
    if (m_active) {
        if (m_app) {
            m_app->notify(QStringLiteral("Library enrichment is already running"),
                          QStringLiteral("info"));
        }
        return false;
    }
    if (!m_discogs || !m_discogs->hasToken()) {
        if (m_app) {
            m_app->notify(QStringLiteral("Add a Discogs token in Integrations first"),
                          QStringLiteral("error"));
        }
        return false;
    }
    if (m_discogs->busy()) {
        if (m_app) {
            m_app->notify(QStringLiteral("Discogs is busy"), QStringLiteral("info"));
        }
        return false;
    }
    if (m_lyrics && m_lyrics->busy()) {
        if (m_app) {
            m_app->notify(QStringLiteral("Lyrics fetch is already running"), QStringLiteral("info"));
        }
        return false;
    }
    if (m_metadataSearch && m_metadataSearch->searching()) {
        if (m_app) {
            m_app->notify(QStringLiteral("Metadata search is already running"),
                          QStringLiteral("info"));
        }
        return false;
    }
    if (m_app) {
        if (m_app->bulkDiscogsAlbumsActive()) {
            m_app->notify(QStringLiteral("Bulk Discogs album fetch is already running"),
                          QStringLiteral("info"));
            return false;
        }
        if (m_app->albumDiscogsOpen()) {
            m_app->notify(QStringLiteral("Close the Discogs album dialog first"),
                          QStringLiteral("info"));
            return false;
        }
        if (m_app->titleFixOpen()) {
            m_app->notify(QStringLiteral("Close the Title Fix dialog first"),
                          QStringLiteral("info"));
            return false;
        }
    }

    m_artistQueue = m_library->artists();
    m_albumQueue.clear();
    m_titleFixQueue.clear();
    for (const QString &artist : m_artistQueue) {
        const QStringList albums = m_library->albumsForArtist(artist);
        for (const QString &album : albums) {
            m_albumQueue.append({artist, album});
            m_titleFixQueue.append({artist, album});
        }
    }

    if (m_artistQueue.isEmpty() && m_albumQueue.isEmpty()) {
        if (m_app) {
            m_app->notify(QStringLiteral("Library is empty — nothing to enrich"),
                          QStringLiteral("info"));
        }
        return false;
    }

    m_active = true;
    m_paused = false;
    m_cancelRequested = false;
    m_autoApplied = 0;
    m_resolved = 0;
    m_skipped = 0;
    m_failed = 0;
    m_artistIndex = 0;
    m_albumIndex = 0;
    m_titleFixIndex = 0;
    m_wait = WaitKind::None;
    m_pendingAutoApply = false;
    m_pendingFolder.clear();
    clearResolveUi();
    m_currentArtist.clear();
    m_currentAlbum.clear();

    if (m_app) {
        m_app->notify(QStringLiteral("Library enrichment started"), QStringLiteral("info"));
    }
    startArtistPhase();
    return true;
}

void LibraryEnrichmentService::cancel()
{
    if (!m_active) {
        return;
    }
    m_cancelRequested = true;
    if (m_wait == WaitKind::Lyrics && m_lyrics) {
        m_lyrics->cancel();
    }
    if (m_paused) {
        m_paused = false;
        clearResolveUi();
    }
    setStatus(QStringLiteral("Cancelled — kept already written sidecars and tags"));
    finishRun();
    if (m_app) {
        m_app->notify(QStringLiteral("Library enrichment cancelled"), QStringLiteral("info"));
    }
}

void LibraryEnrichmentService::chooseSelected()
{
    if (!m_active || !m_paused) {
        return;
    }

    if (m_resolveKind == QStringLiteral("artist")) {
        if (m_selectedIndex < 0 || m_selectedIndex >= m_candidates.size()) {
            return;
        }
        const quint64 id =
            m_candidates.at(m_selectedIndex).toMap().value(QStringLiteral("id")).toULongLong();
        m_paused = false;
        clearResolveUi();
        emitState();
        applyArtistChoice(id, false);
        return;
    }

    if (m_resolveKind == QStringLiteral("album")) {
        if (m_selectedIndex < 0 || m_selectedIndex >= m_candidates.size()) {
            return;
        }
        const quint64 id =
            m_candidates.at(m_selectedIndex).toMap().value(QStringLiteral("id")).toULongLong();
        m_paused = false;
        clearResolveUi();
        emitState();
        applyAlbumChoice(id, false);
        return;
    }

    if (m_resolveKind == QStringLiteral("titleFix")) {
        m_paused = false;
        clearResolveUi();
        emitState();
        applyTitleFixProposals(false);
        ++m_titleFixIndex;
        ++m_progress;
        scheduleAdvance([this]() { processNextTitleFix(); });
    }
}

void LibraryEnrichmentService::skipCurrent()
{
    if (!m_active || !m_paused) {
        return;
    }

    const QString kind = m_resolveKind;
    m_paused = false;
    clearResolveUi();
    ++m_skipped;

    if (kind == QStringLiteral("artist")) {
        ++m_artistIndex;
        ++m_progress;
        setStatus(QStringLiteral("Skipped artist"));
        emitState();
        scheduleAdvance([this]() { processNextArtist(); });
        return;
    }
    if (kind == QStringLiteral("album")) {
        ++m_albumIndex;
        ++m_progress;
        setStatus(QStringLiteral("Skipped album"));
        emitState();
        scheduleAdvance([this]() { processNextAlbum(); });
        return;
    }
    if (kind == QStringLiteral("titleFix")) {
        ++m_titleFixIndex;
        ++m_progress;
        setStatus(QStringLiteral("Skipped title fix"));
        emitState();
        scheduleAdvance([this]() { processNextTitleFix(); });
    }
}

void LibraryEnrichmentService::setSelectedIndex(int index)
{
    const int bounded = qBound(-1, index, m_candidates.size() - 1);
    if (m_selectedIndex == bounded) {
        return;
    }
    m_selectedIndex = bounded;
    if (m_paused && m_resolveKind == QStringLiteral("titleFix") && m_metadataSearch) {
        m_metadataSearch->setSelectedIndex(bounded);
        mirrorTitleFixCandidates();
    }
    emitState();
}

void LibraryEnrichmentService::setTitleFixChecked(int index, bool checked)
{
    if (m_metadataSearch) {
        m_metadataSearch->setTitleFixChecked(index, checked);
        emitState();
    }
}

void LibraryEnrichmentService::handleAlbumSearchResults(const QVariantList &results)
{
    onAlbumSearchResults(results);
}

void LibraryEnrichmentService::setStatus(const QString &status)
{
    if (m_status == status) {
        return;
    }
    m_status = status;
}

void LibraryEnrichmentService::emitState()
{
    emit stateChanged();
}

void LibraryEnrichmentService::clearResolveUi()
{
    m_resolveKind = QStringLiteral("none");
    m_candidates.clear();
    m_selectedIndex = -1;
    m_localTrackCount = 0;
}

void LibraryEnrichmentService::finishRun()
{
    m_active = false;
    m_paused = false;
    m_wait = WaitKind::None;
    m_pendingAutoApply = false;
    m_pendingFolder.clear();
    m_cancelRequested = false;
    if (m_phase != QStringLiteral("done")) {
        m_phase = QStringLiteral("done");
    }
    clearResolveUi();
    emitState();
}

void LibraryEnrichmentService::bumpFailedAndAdvance()
{
    ++m_failed;
    if (m_phase == QStringLiteral("artist")) {
        ++m_artistIndex;
        ++m_progress;
        emitState();
        scheduleAdvance([this]() { processNextArtist(); });
        return;
    }
    if (m_phase == QStringLiteral("album")) {
        ++m_albumIndex;
        ++m_progress;
        emitState();
        scheduleAdvance([this]() { processNextAlbum(); });
        return;
    }
    if (m_phase == QStringLiteral("titleFix")) {
        ++m_titleFixIndex;
        ++m_progress;
        emitState();
        scheduleAdvance([this]() { processNextTitleFix(); });
    }
}

void LibraryEnrichmentService::advanceAfterSuccess(bool autoApplied)
{
    if (autoApplied) {
        ++m_autoApplied;
    } else {
        ++m_resolved;
    }
    if (m_phase == QStringLiteral("artist")) {
        ++m_artistIndex;
        ++m_progress;
        emitState();
        scheduleAdvance([this]() { processNextArtist(); });
        return;
    }
    if (m_phase == QStringLiteral("album")) {
        ++m_albumIndex;
        ++m_progress;
        emitState();
        scheduleAdvance([this]() { processNextAlbum(); });
    }
}

void LibraryEnrichmentService::scheduleAdvance(std::function<void()> next)
{
    if (m_cancelRequested || !m_active) {
        return;
    }
    QTimer::singleShot(kDiscogsStepDelayMs, this, [this, next]() {
        if (m_cancelRequested || !m_active) {
            return;
        }
        next();
    });
}

void LibraryEnrichmentService::startArtistPhase()
{
    m_phase = QStringLiteral("artist");
    m_progress = 0;
    m_total = m_artistQueue.size();
    m_artistIndex = 0;
    setStatus(m_total == 0 ? QStringLiteral("No artists — skipping to albums")
                           : QStringLiteral("Artist info: searching Discogs…"));
    emitState();
    processNextArtist();
}

void LibraryEnrichmentService::processNextArtist()
{
    if (m_cancelRequested || !m_active) {
        return;
    }
    while (m_artistIndex < m_artistQueue.size()) {
        m_currentArtist = m_artistQueue.at(m_artistIndex);
        m_currentAlbum.clear();
        const QString samplePath = m_library->sampleTrackPathForArtist(m_currentArtist);
        const QString folder = m_library->artistFolderForTrack(samplePath);
        if (folder.isEmpty()) {
            setStatus(QStringLiteral("No folder for artist \"%1\"").arg(m_currentArtist));
            ++m_failed;
            ++m_artistIndex;
            ++m_progress;
            emitState();
            continue;
        }
        if (artistFolderHasInfo(folder)) {
            setStatus(QStringLiteral("Artist %1/%2: \"%3\" already has info — skipping")
                          .arg(m_artistIndex + 1)
                          .arg(m_total)
                          .arg(m_currentArtist));
            ++m_skipped;
            ++m_artistIndex;
            ++m_progress;
            emitState();
            continue;
        }
        m_pendingFolder = folder;
        m_wait = WaitKind::ArtistSearch;
        setStatus(QStringLiteral("Artist %1/%2: searching \"%3\"")
                      .arg(m_artistIndex + 1)
                      .arg(m_total)
                      .arg(m_currentArtist));
        emitState();
        m_discogs->searchArtists(m_currentArtist);
        return;
    }
    startAlbumPhase();
}

QVariantList LibraryEnrichmentService::rankArtistCandidates(const QVariantList &results,
                                                            const QString &artist)
{
    QVariantList exact;
    QVariantList other;
    for (const QVariant &value : results) {
        QVariantMap row = value.toMap();
        const QString title = row.value(QStringLiteral("title")).toString();
        const bool isExact = title.compare(artist, Qt::CaseInsensitive) == 0;
        row.insert(QStringLiteral("badge"),
                   isExact ? QStringLiteral("exact") : QString());
        if (isExact) {
            exact << row;
        } else {
            other << row;
        }
    }
    return exact + other;
}

void LibraryEnrichmentService::onArtistsSearchFinished(const QVariantList &results)
{
    if (!m_active || m_wait != WaitKind::ArtistSearch || m_phase != QStringLiteral("artist")) {
        return;
    }
    m_wait = WaitKind::None;

    const QVariantList ranked = rankArtistCandidates(results, m_currentArtist);
    QVariantList exactOnly;
    for (const QVariant &value : ranked) {
        if (value.toMap().value(QStringLiteral("badge")).toString() == QStringLiteral("exact")) {
            exactOnly << value;
        }
    }

    // Any exact title match auto-applies the best-ranked exact.
    if (!exactOnly.isEmpty()) {
        const quint64 id = exactOnly.first().toMap().value(QStringLiteral("id")).toULongLong();
        applyArtistChoice(id, true);
        return;
    }

    if (ranked.isEmpty()) {
        setStatus(QStringLiteral("No Discogs artist matches for \"%1\"").arg(m_currentArtist));
        bumpFailedAndAdvance();
        return;
    }

    m_candidates = ranked;
    m_selectedIndex = 0;
    m_resolveKind = QStringLiteral("artist");
    m_paused = true;
    m_localTrackCount = 0;
    setStatus(QStringLiteral("Choose Discogs artist for \"%1\"").arg(m_currentArtist));
    emitState();
}

void LibraryEnrichmentService::applyArtistChoice(quint64 discogsId, bool autoApplied)
{
    if (discogsId == 0 || m_pendingFolder.isEmpty()) {
        bumpFailedAndAdvance();
        return;
    }
    m_pendingAutoApply = autoApplied;
    m_wait = WaitKind::ArtistFetch;
    setStatus(QStringLiteral("Fetching artist \"%1\"…").arg(m_currentArtist));
    emitState();
    m_discogs->fetchArtist(m_currentArtist, m_pendingFolder, discogsId);
}

void LibraryEnrichmentService::onArtistFetched(const QString &artistName, bool success)
{
    if (!m_active || m_wait != WaitKind::ArtistFetch || m_phase != QStringLiteral("artist")) {
        return;
    }
    if (artistName != m_currentArtist) {
        return;
    }
    m_wait = WaitKind::None;
    if (!success) {
        setStatus(QStringLiteral("Failed to fetch artist \"%1\"").arg(artistName));
        bumpFailedAndAdvance();
        return;
    }
    setStatus(QStringLiteral("Saved artist info for \"%1\"").arg(artistName));
    advanceAfterSuccess(m_pendingAutoApply);
}

void LibraryEnrichmentService::startAlbumPhase()
{
    m_phase = QStringLiteral("album");
    m_progress = 0;
    m_total = m_albumQueue.size();
    m_albumIndex = 0;
    m_currentArtist.clear();
    m_currentAlbum.clear();
    setStatus(m_total == 0 ? QStringLiteral("No albums — skipping to title fix")
                           : QStringLiteral("Album info: searching Discogs…"));
    emitState();
    processNextAlbum();
}

void LibraryEnrichmentService::processNextAlbum()
{
    if (m_cancelRequested || !m_active) {
        return;
    }
    while (m_albumIndex < m_albumQueue.size()) {
        const auto pair = m_albumQueue.at(m_albumIndex);
        m_currentArtist = pair.first;
        m_currentAlbum = pair.second;
        const QVariantList tracks =
            m_library->tracksForAlbum(m_currentArtist, m_currentAlbum);
        if (tracks.isEmpty()) {
            setStatus(QStringLiteral("No tracks for \"%1 — %2\"")
                          .arg(m_currentArtist, m_currentAlbum));
            ++m_failed;
            ++m_albumIndex;
            ++m_progress;
            emitState();
            continue;
        }
        m_localTrackCount = tracks.size();
        m_pendingFolder =
            QFileInfo(tracks.first().toMap().value(QStringLiteral("path")).toString())
                .absolutePath();
        if (albumFolderHasInfo(m_pendingFolder)) {
            setStatus(QStringLiteral("Album %1/%2: \"%3 — %4\" already has info — skipping")
                          .arg(m_albumIndex + 1)
                          .arg(m_total)
                          .arg(m_currentArtist, m_currentAlbum));
            ++m_skipped;
            ++m_albumIndex;
            ++m_progress;
            emitState();
            continue;
        }
        m_wait = WaitKind::AlbumSearch;
        setStatus(QStringLiteral("Album %1/%2: searching \"%3 — %4\"")
                      .arg(m_albumIndex + 1)
                      .arg(m_total)
                      .arg(m_currentArtist, m_currentAlbum));
        emitState();
        m_discogs->searchReleases(m_currentArtist, m_currentAlbum,
                                  QStringList{QStringLiteral("CD"), QStringLiteral("Digital")},
                                  {});
        return;
    }
    startTitleFixPhase();
}

void LibraryEnrichmentService::onAlbumSearchResults(const QVariantList &results)
{
    if (!m_active || m_wait != WaitKind::AlbumSearch || m_phase != QStringLiteral("album")) {
        return;
    }
    m_wait = WaitKind::None;

    const QVariantList ranked = rankDiscogsAlbumCandidates(results, m_localTrackCount);
    QList<int> exactIndexes;
    QVariantList decorated;
    for (int i = 0; i < ranked.size(); ++i) {
        QVariantMap row = ranked.at(i).toMap();
        row.insert(QStringLiteral("localTrackCount"), m_localTrackCount);
        const QString key = albumTitleKey(row.value(QStringLiteral("title")).toString(),
                                          m_currentArtist);
        const bool isExact = key.compare(m_currentAlbum, Qt::CaseInsensitive) == 0;
        if (isExact) {
            exactIndexes << decorated.size();
            row.insert(QStringLiteral("badge"), QStringLiteral("exact"));
        } else {
            const int media = row.value(QStringLiteral("mediaCount")).toInt();
            row.insert(QStringLiteral("badge"),
                       media > 0 ? QStringLiteral("%1×").arg(media) : QString());
        }
        decorated << row;
    }

    // Any exact title match auto-applies the best-ranked exact (CD / disc-count ranking).
    if (!exactIndexes.isEmpty()) {
        const quint64 id =
            decorated.at(exactIndexes.first()).toMap().value(QStringLiteral("id")).toULongLong();
        applyAlbumChoice(id, true);
        return;
    }

    if (decorated.isEmpty()) {
        setStatus(QStringLiteral("No Discogs album matches for \"%1 — %2\"")
                      .arg(m_currentArtist, m_currentAlbum));
        bumpFailedAndAdvance();
        return;
    }

    m_candidates = decorated;
    m_selectedIndex = 0;
    m_resolveKind = QStringLiteral("album");
    m_paused = true;
    setStatus(QStringLiteral("Choose Discogs album for \"%1 — %2\"")
                  .arg(m_currentArtist, m_currentAlbum));
    emitState();
}

void LibraryEnrichmentService::applyAlbumChoice(quint64 releaseId, bool autoApplied)
{
    if (releaseId == 0 || m_pendingFolder.isEmpty()) {
        bumpFailedAndAdvance();
        return;
    }
    m_pendingAutoApply = autoApplied;
    m_wait = WaitKind::AlbumFetch;
    setStatus(QStringLiteral("Fetching album \"%1 — %2\"…")
                  .arg(m_currentArtist, m_currentAlbum));
    emitState();
    m_discogs->fetchRelease(m_currentArtist, m_currentAlbum, m_pendingFolder, releaseId);
}

void LibraryEnrichmentService::onReleaseFetched(const QString &artist, const QString &album,
                                                bool success)
{
    if (!m_active || m_wait != WaitKind::AlbumFetch || m_phase != QStringLiteral("album")) {
        return;
    }
    if (artist != m_currentArtist || album != m_currentAlbum) {
        return;
    }
    m_wait = WaitKind::None;
    if (!success) {
        setStatus(QStringLiteral("Failed to fetch album \"%1 — %2\"").arg(artist, album));
        bumpFailedAndAdvance();
        return;
    }
    setStatus(QStringLiteral("Saved album info for \"%1 — %2\"").arg(artist, album));
    advanceAfterSuccess(m_pendingAutoApply);
}

QString LibraryEnrichmentService::albumTitleKey(const QString &title, const QString &artist)
{
    const QString prefix = artist + QStringLiteral(" - ");
    if (title.startsWith(prefix, Qt::CaseInsensitive)) {
        return title.mid(prefix.size()).trimmed();
    }
    return title.trimmed();
}

bool LibraryEnrichmentService::isRedirectProfile(const QString &profile)
{
    const QString lower = profile.toLower();
    return lower.contains(QStringLiteral("please use"))
           || lower.contains(QStringLiteral("please see"))
           || lower.contains(QStringLiteral("see artist"))
           || (lower.contains(QStringLiteral("for band"))
               && lower.contains(QStringLiteral("use ")))
           || (lower.contains(QStringLiteral("for the band"))
               && lower.contains(QStringLiteral("use ")));
}

bool LibraryEnrichmentService::artistFolderHasInfo(const QString &folder)
{
    if (folder.isEmpty()) {
        return false;
    }
    QFile profile(folder + QStringLiteral("/profile.txt"));
    if (!profile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    const QString text = QString::fromUtf8(profile.readAll()).trimmed();
    return !text.isEmpty() && !isRedirectProfile(text);
}

bool LibraryEnrichmentService::albumFolderHasInfo(const QString &folder)
{
    if (folder.isEmpty()) {
        return false;
    }
    QFile info(folder + QStringLiteral("/album-info.txt"));
    if (!info.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }
    return !QString::fromUtf8(info.readAll()).trimmed().isEmpty();
}

void LibraryEnrichmentService::startTitleFixPhase()
{
    m_phase = QStringLiteral("titleFix");
    m_progress = 0;
    m_total = m_titleFixQueue.size();
    m_titleFixIndex = 0;
    m_currentArtist.clear();
    m_currentAlbum.clear();
    setStatus(m_total == 0 ? QStringLiteral("No albums — skipping to lyrics")
                           : QStringLiteral("Title Fix: matching releases…"));
    emitState();
    processNextTitleFix();
}

void LibraryEnrichmentService::processNextTitleFix()
{
    if (m_cancelRequested || !m_active) {
        return;
    }
    while (m_titleFixIndex < m_titleFixQueue.size()) {
        const auto pair = m_titleFixQueue.at(m_titleFixIndex);
        m_currentArtist = pair.first;
        m_currentAlbum = pair.second;
        const QVariantList tracks =
            m_library->tracksForAlbum(m_currentArtist, m_currentAlbum);
        if (tracks.isEmpty()) {
            setStatus(QStringLiteral("No tracks for title fix \"%1 — %2\"")
                          .arg(m_currentArtist, m_currentAlbum));
            ++m_failed;
            ++m_titleFixIndex;
            ++m_progress;
            emitState();
            continue;
        }
        m_localTrackCount = tracks.size();
        m_wait = WaitKind::TitleFixSearch;
        setStatus(QStringLiteral("Title Fix %1/%2: searching \"%3 — %4\"")
                      .arg(m_titleFixIndex + 1)
                      .arg(m_total)
                      .arg(m_currentArtist, m_currentAlbum));
        emitState();
        m_metadataSearch->clear();
        m_metadataSearch->setLocalTracksForTitleFix(tracks);
        QVariantMap current;
        current.insert(QStringLiteral("artist"), m_currentArtist);
        current.insert(QStringLiteral("album"), m_currentAlbum);
        m_metadataSearch->setCurrentFields(current);
        m_metadataSearch->searchRelease(
            m_currentArtist, m_currentAlbum, {},
            QVariantList{QStringLiteral("CD"), QStringLiteral("Digital")}, {});
        return;
    }
    startLyricsPhase();
}

void LibraryEnrichmentService::onTitleFixSearchFinished(bool success)
{
    Q_UNUSED(success);
    if (!m_active || m_wait != WaitKind::TitleFixSearch || m_phase != QStringLiteral("titleFix")) {
        return;
    }

    // Defer so setSelectedIndex(0) / detail fetch from finishSearchIfReady fully settle.
    QTimer::singleShot(0, this, [this]() {
        if (!m_active || m_phase != QStringLiteral("titleFix")
            || m_wait != WaitKind::TitleFixSearch) {
            return;
        }
        if (m_metadataSearch->candidates().isEmpty()) {
            m_wait = WaitKind::None;
            setStatus(QStringLiteral("No title-fix releases for \"%1 — %2\"")
                          .arg(m_currentArtist, m_currentAlbum));
            bumpFailedAndAdvance();
            return;
        }
        mirrorTitleFixCandidates();
        if (m_metadataSearch->status().startsWith(QStringLiteral("Loading release details"))) {
            m_wait = WaitKind::TitleFixDetail;
            setStatus(QStringLiteral("Loading release details for \"%1 — %2\"…")
                          .arg(m_currentArtist, m_currentAlbum));
            emitState();
            return;
        }
        m_wait = WaitKind::None;
        decideTitleFixAutoOrPause();
    });
}

void LibraryEnrichmentService::onTitleFixDetailMaybeReady()
{
    if (!m_active || m_wait != WaitKind::TitleFixDetail) {
        return;
    }
    if (m_metadataSearch->status().startsWith(QStringLiteral("Loading release details"))) {
        return;
    }
    m_wait = WaitKind::None;
    mirrorTitleFixCandidates();
    decideTitleFixAutoOrPause();
}

void LibraryEnrichmentService::mirrorTitleFixCandidates()
{
    m_candidates = m_metadataSearch->candidates();
    m_selectedIndex = m_metadataSearch->selectedIndex();
    m_localTrackCount = m_metadataSearch->localTrackCount();
}

bool LibraryEnrichmentService::titleFixIsExactAutoApply() const
{
    if (!m_metadataSearch || m_metadataSearch->candidates().isEmpty()) {
        return false;
    }
    const int idx = m_metadataSearch->selectedIndex();
    if (idx < 0 || idx >= m_metadataSearch->candidates().size()) {
        return false;
    }
    const QVariantMap row = m_metadataSearch->candidates().at(idx).toMap();
    const int trackCount = row.value(QStringLiteral("trackCount")).toInt();
    if (trackCount <= 0 || trackCount != m_metadataSearch->localTrackCount()) {
        return false;
    }
    if (!m_metadataSearch->titleFixUnmatched().isEmpty()) {
        return false;
    }
    const QVariantList proposals = m_metadataSearch->titleFixProposals();
    for (const QVariant &value : proposals) {
        if (value.toMap().value(QStringLiteral("matchScore")).toInt() < 95) {
            return false;
        }
    }
    return true;
}

void LibraryEnrichmentService::decideTitleFixAutoOrPause()
{
    if (titleFixIsExactAutoApply()) {
        const QVariantList proposals = m_metadataSearch->titleFixProposals();
        if (proposals.isEmpty() && m_metadataSearch->titleFixUnmatched().isEmpty()) {
            setStatus(QStringLiteral("Titles already match for \"%1 — %2\"")
                          .arg(m_currentArtist, m_currentAlbum));
            ++m_autoApplied;
            ++m_titleFixIndex;
            ++m_progress;
            emitState();
            scheduleAdvance([this]() { processNextTitleFix(); });
            return;
        }
        applyTitleFixProposals(true);
        ++m_titleFixIndex;
        ++m_progress;
        emitState();
        scheduleAdvance([this]() { processNextTitleFix(); });
        return;
    }

    mirrorTitleFixCandidates();
    m_resolveKind = QStringLiteral("titleFix");
    m_paused = true;
    setStatus(QStringLiteral("Review title fix for \"%1 — %2\"")
                  .arg(m_currentArtist, m_currentAlbum));
    emitState();
}

void LibraryEnrichmentService::applyTitleFixProposals(bool countAsAuto)
{
    const QVariantList proposals = m_metadataSearch->checkedTitleFixProposals();
    int updated = 0;
    int failed = 0;
    for (const QVariant &proposalValue : proposals) {
        const QVariantMap proposal = proposalValue.toMap();
        const QString path = proposal.value(QStringLiteral("path")).toString();
        const QString title = proposal.value(QStringLiteral("proposed")).toString().trimmed();
        if (path.isEmpty() || title.isEmpty()) {
            ++failed;
            continue;
        }
        QVariantMap fields;
        fields.insert(QStringLiteral("title"), title);
        if (!m_tags->saveTrackTags(path, fields)) {
            ++failed;
            continue;
        }
        ++updated;
    }

    if (countAsAuto) {
        ++m_autoApplied;
    } else {
        ++m_resolved;
    }
    if (failed > 0) {
        m_failed += failed;
    }
    setStatus(updated > 0
                  ? QStringLiteral("Applied %1 title change(s) on \"%2 — %3\"")
                        .arg(updated)
                        .arg(m_currentArtist, m_currentAlbum)
                  : QStringLiteral("No title changes applied for \"%1 — %2\"")
                        .arg(m_currentArtist, m_currentAlbum));
}

void LibraryEnrichmentService::startLyricsPhase()
{
    m_phase = QStringLiteral("lyrics");
    m_currentArtist.clear();
    m_currentAlbum.clear();
    clearResolveUi();

    QVariantList tracks;
    for (const QString &artist : m_artistQueue) {
        tracks.append(m_library->tracksForArtist(artist));
    }
    m_progress = 0;
    m_total = tracks.size();
    if (tracks.isEmpty()) {
        setStatus(QStringLiteral("No tracks for lyrics"));
        m_phase = QStringLiteral("done");
        finishRun();
        if (m_app) {
            m_app->notify(QStringLiteral("Library enrichment complete"),
                          QStringLiteral("success"));
        }
        return;
    }

    m_wait = WaitKind::Lyrics;
    setStatus(QStringLiteral("Fetching lyrics for %1 tracks…").arg(tracks.size()));
    emitState();
    m_lyrics->fetchForTracks(tracks);
}

void LibraryEnrichmentService::onLyricsBusyChanged()
{
    if (!m_active || m_wait != WaitKind::Lyrics || m_phase != QStringLiteral("lyrics")) {
        return;
    }
    if (m_lyrics->busy()) {
        return;
    }
    m_wait = WaitKind::None;
    m_progress = m_lyrics->progress();
    m_total = m_lyrics->total();
    setStatus(m_lyrics->status().isEmpty() ? QStringLiteral("Lyrics phase complete")
                                           : m_lyrics->status());
    m_phase = QStringLiteral("done");
    finishRun();
    if (m_app && !m_cancelRequested) {
        m_app->notify(QStringLiteral("Library enrichment complete"), QStringLiteral("success"));
    }
}

void LibraryEnrichmentService::onLyricsProgressChanged()
{
    if (!m_active || m_phase != QStringLiteral("lyrics")) {
        return;
    }
    m_progress = m_lyrics->progress();
    m_total = m_lyrics->total();
    setStatus(QStringLiteral("Lyrics %1/%2").arg(m_progress).arg(m_total));
    emitState();
}
