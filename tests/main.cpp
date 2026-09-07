int runFuzzyMatchTests(int argc, char **argv);
int runLrcParserTests(int argc, char **argv);
int runPlaylistM3uTests(int argc, char **argv);
int runLyricsParsersTests(int argc, char **argv);

int main(int argc, char **argv) {
    int status = 0;
    status |= runFuzzyMatchTests(argc, argv);
    status |= runLrcParserTests(argc, argv);
    status |= runPlaylistM3uTests(argc, argv);
    status |= runLyricsParsersTests(argc, argv);
    return status;
}
