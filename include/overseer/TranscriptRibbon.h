#pragma once

#include <QList>
#include <QWidget>

#include "TranscriptEvent.h"

class TranscriptRibbon : public QWidget {
  Q_OBJECT

public:
  explicit TranscriptRibbon(QWidget *parent = nullptr);

  void setEvents(const QList<TranscriptEvent> &events);

  // The event indices currently visible in the transcript. Both ends
  // are inclusive. Used to draw the cursor. Nothing about the ribbon
  // limits the transcript; this is purely visual feedback for the
  // navigation map.
  void setVisibleRange(int startIndex, int endIndex);

  signals:
    // Emitted when the user clicks or drags the ribbon. The caller
    // should scroll the transcript so that the event at `index` is
    // brought into view.
    void jumpRequested(int index);

protected:
  void paintEvent(QPaintEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void changeEvent(QEvent *event) override;

private:
  int indexAtX(int x) const;
  int xForIndex(int index) const;
  int xForIndexEnd(int index) const;

  QList<TranscriptEvent> m_events;
  int m_total = 0;

  int m_visibleStart = 0;
  int m_visibleEnd = 0;

  bool m_draggingCursor = false;

  static constexpr int kRibbonHeight = 28;
  static constexpr int kTickGap = 2;
  static constexpr int kCursorWidthPx = 3;
  static constexpr int kCursorHitZonePx = 8;
};