#ifndef EPISTEME_DOCUMENTMODE_H
#define EPISTEME_DOCUMENTMODE_H

enum class DocumentMode {
  Markdown,
  Html,
  PlainText,
  Dot,
  PlantUml,
  Mermaid,
  // Media kinds. These are never opened as TextDocuments; DocumentManager
  // intercepts them and routes them to MediaPane instead.
  RasterImage,
  VectorImage,
  Audio,
  Video
};

#endif // EPISTEME_DOCUMENTMODE_H