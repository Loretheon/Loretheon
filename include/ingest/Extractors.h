#ifndef EPISTEME_EXTRACTORS_H
#define EPISTEME_EXTRACTORS_H

#include <memory>

class Extractor;

std::unique_ptr<Extractor> makeMarkdownExtractor();
std::unique_ptr<Extractor> makePdfExtractor();
std::unique_ptr<Extractor> makeHtmlExtractor();
std::unique_ptr<Extractor> makeDocxExtractor();
std::unique_ptr<Extractor> makePptxExtractor();
std::unique_ptr<Extractor> makeEpubExtractor();

#endif // EPISTEME_EXTRACTORS_H