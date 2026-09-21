#ifndef EPISTEME_EXTRACTORS_H
#define EPISTEME_EXTRACTORS_H

#include <memory>

class Extractor;

// Factory functions for the built-in extractors. Each returns a
// heap-allocated extractor owned by the caller. Defined in the
// corresponding src/ingest/*Extractor.cpp file.
std::unique_ptr<Extractor> makePdfExtractor();
std::unique_ptr<Extractor> makeHtmlExtractor();
std::unique_ptr<Extractor> makeDocxExtractor();
std::unique_ptr<Extractor> makePptxExtractor();
std::unique_ptr<Extractor> makeEpubExtractor();

#endif // EPISTEME_EXTRACTORS_H