#include "../../include/assistant/AssistantWidget.h"

#include "../../include/app/theme/ThemeRegistry.h"
#include "../../include/assistant/LoreAssistant.h"
#include "../../include/voice/SpeechController.h"

#include <QApplication>
#include <QComboBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QScrollBar>
#include <QSettings>
#include <QStyle>
#include <QTextEdit>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kPanelMargin = 24;
constexpr int kPanelDefaultWidth = 440;
constexpr int kPanelMinWidth = 320;
constexpr int kPanelMinHeight = 280;
constexpr int kPanelDefaultHeightFractionNum = 3;
constexpr int kPanelDefaultHeightFractionDen = 4;

constexpr int kCornerBand = 12;
constexpr int kEdgeBand = 6;

constexpr int kCardAlpha = 235;
constexpr int kCardRadius = 12;
constexpr int kDocumentMargin = 18;

constexpr auto kPanelPosKey = "assistant/panelPos";
constexpr auto kPolicyKey = "assistant/completionPolicy";

} // namespace

AssistantWidget::AssistantWidget(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("assistantPanel"));

  setWindowFlags(Qt::Tool | Qt::FramelessWindowHint |
                 Qt::NoDropShadowWindowHint |
                 Qt::WindowStaysOnTopHint);

#ifdef Q_OS_LINUX
  setWindowFlags(windowFlags() | Qt::X11BypassWindowManagerHint);
#endif

  setAttribute(Qt::WA_TranslucentBackground, true);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setAutoFillBackground(false);

  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setVisible(false);

  m_tokens = ThemeRegistry::instance().tokens(
      ThemeRegistry::instance().activeTheme());

  buildUi();
  restoreSavedPosition();
}

AssistantWidget::~AssistantWidget() = default;

void AssistantWidget::setThemeTokens(const ThemeTokens &tokens) {
  m_tokens = tokens;
  renderTranscript();
  update();
}

void AssistantWidget::setAssistant(LoreAssistant *assistant) {
  m_assistant = assistant;

  if (!m_assistant || !m_policy) {
    return;
  }

  QSettings settings;
  const int stored = settings
                         .value(kPolicyKey,
                                static_cast<int>(
                                    LoreAssistant::CompletionPolicy::Automatic))
                         .toInt();

  const int index = m_policy->findData(stored);

  if (index >= 0) {
    m_policy->blockSignals(true);
    m_policy->setCurrentIndex(index);
    m_policy->blockSignals(false);
  }

  m_assistant->setCompletionPolicy(
      static_cast<LoreAssistant::CompletionPolicy>(stored));
}

void AssistantWidget::setSpeechController(SpeechController *speech) {
  if (m_speech) {
    disconnect(m_speech, nullptr, this, nullptr);
  }

  m_speech = speech;

  if (!m_speech) {
    return;
  }

  connect(m_speech, &SpeechController::transcribed, this,
          &AssistantWidget::onTranscribed);
  connect(m_speech, &SpeechController::liveTranscribed, this,
          &AssistantWidget::onLiveTranscribed);
  connect(m_speech, &SpeechController::stateChanged, this,
          &AssistantWidget::onSpeechStateChanged);

  onSpeechStateChanged();
}

void AssistantWidget::buildUi() {
  m_card = new QFrame(this);
  m_card->setObjectName(QStringLiteral("assistantCard"));
  m_card->setAttribute(Qt::WA_TranslucentBackground, true);
  m_card->setAttribute(Qt::WA_NoSystemBackground, true);
  m_card->setAutoFillBackground(false);

  m_header = new QWidget(m_card);
  m_header->setObjectName(QStringLiteral("assistantHeader"));
  m_header->setAttribute(Qt::WA_TranslucentBackground, true);
  m_header->setAutoFillBackground(false);

  m_title = new QLabel(tr("Lore"), m_header);
  m_title->setObjectName(QStringLiteral("assistantTitle"));
  m_title->setAttribute(Qt::WA_TranslucentBackground, true);

  m_policy = new QComboBox(m_header);
  m_policy->setObjectName(QStringLiteral("assistantPolicy"));
  m_policy->addItem(tr("Results: automatic"),
                    static_cast<int>(
                        LoreAssistant::CompletionPolicy::Automatic));
  m_policy->addItem(tr("Results: paste in chat"),
                    static_cast<int>(
                        LoreAssistant::CompletionPolicy::PasteInChat));
  m_policy->addItem(tr("Results: feed to Lore"),
                    static_cast<int>(
                        LoreAssistant::CompletionPolicy::FeedToQueue));
  m_policy->addItem(tr("Results: next message"),
                    static_cast<int>(
                        LoreAssistant::CompletionPolicy::AppendToNextUserMessage));
  m_policy->setToolTip(
      tr("What to do when a background task finishes."));

  m_close = new QPushButton(QStringLiteral("✕"), m_header);
  m_close->setObjectName(QStringLiteral("assistantClose"));
  m_close->setCursor(Qt::PointingHandCursor);
  m_close->setFlat(true);
  m_close->setFixedSize(28, 28);

  auto *headerLayout = new QHBoxLayout(m_header);
  headerLayout->setContentsMargins(18, 12, 12, 12);
  headerLayout->setSpacing(8);
  headerLayout->addWidget(m_title);
  headerLayout->addStretch(1);
  headerLayout->addWidget(m_policy);
  headerLayout->addWidget(m_close);

  m_transcript = new QTextEdit(m_card);
  m_transcript->setObjectName(QStringLiteral("transcript"));
  m_transcript->setReadOnly(true);
  m_transcript->setAcceptRichText(true);
  m_transcript->setLineWrapMode(QTextEdit::WidgetWidth);
  m_transcript->setFrameShape(QFrame::NoFrame);
  m_transcript->setAttribute(Qt::WA_TranslucentBackground, true);
  m_transcript->setAutoFillBackground(false);
  m_transcript->viewport()->setAttribute(Qt::WA_TranslucentBackground, true);
  m_transcript->viewport()->setAutoFillBackground(false);
  m_transcript->document()->setDocumentMargin(kDocumentMargin);

  m_controls = new QWidget(m_card);
  m_controls->setObjectName(QStringLiteral("assistantControls"));
  m_controls->setAttribute(Qt::WA_TranslucentBackground, true);
  m_controls->setAutoFillBackground(false);

  auto makeSpeechButton = [this](const QString &name,
                                 const QString &glyph,
                                 const QString &tooltip) {
    auto *button = new QToolButton(m_controls);
    button->setObjectName(name);
    button->setText(glyph);
    button->setToolTip(tooltip);
    button->setCursor(Qt::PointingHandCursor);
    button->setAutoRaise(true);
    button->setFixedSize(34, 34);
    return button;
  };

  m_dictate = makeSpeechButton(QStringLiteral("assistantDictate"),
                               QStringLiteral("●"),
                               tr("Dictate: record once, transcribe, send."));

  m_live = makeSpeechButton(QStringLiteral("assistantLive"),
                            QStringLiteral("◉"),
                            tr("Live dictate: stream speech to text."));

  m_readAloud = makeSpeechButton(
      QStringLiteral("assistantReadAloud"), QStringLiteral("▶"),
      tr("Read the last reply aloud."));

  m_input = new QLineEdit(m_controls);
  m_input->setObjectName(QStringLiteral("chatInput"));
  m_input->setPlaceholderText(tr("Message"));
  m_input->setClearButtonEnabled(true);

  m_send = new QPushButton(tr("Send"), m_controls);
  m_send->setObjectName(QStringLiteral("sendButton"));
  m_send->setProperty("accent", QStringLiteral("primary"));
  m_send->setCursor(Qt::PointingHandCursor);
  m_send->setDefault(true);

  auto *controlsLayout = new QHBoxLayout(m_controls);
  controlsLayout->setContentsMargins(14, 10, 14, 14);
  controlsLayout->setSpacing(8);
  controlsLayout->addWidget(m_dictate);
  controlsLayout->addWidget(m_live);
  controlsLayout->addWidget(m_readAloud);
  controlsLayout->addSpacing(4);
  controlsLayout->addWidget(m_input, 1);
  controlsLayout->addWidget(m_send);

  auto *cardLayout = new QVBoxLayout(m_card);
  cardLayout->setContentsMargins(0, 0, 0, 0);
  cardLayout->setSpacing(0);
  cardLayout->addWidget(m_header);
  cardLayout->addWidget(m_transcript, 1);
  cardLayout->addWidget(m_controls);

  auto *panelLayout = new QVBoxLayout(this);
  panelLayout->setContentsMargins(0, 0, 0, 0);
  panelLayout->setSpacing(0);
  panelLayout->addWidget(m_card);

  connect(m_send, &QPushButton::clicked, this, &AssistantWidget::onSubmit);
  connect(m_input, &QLineEdit::returnPressed, this,
          &AssistantWidget::onSubmit);
  connect(m_dictate, &QToolButton::clicked, this,
          &AssistantWidget::onDictateClicked);
  connect(m_live, &QToolButton::clicked, this,
          &AssistantWidget::onLiveDictateClicked);
  connect(m_readAloud, &QToolButton::clicked, this,
          &AssistantWidget::onReadAloudClicked);
  connect(m_close, &QPushButton::clicked, this,
          &AssistantWidget::onCloseClicked);

  connect(m_policy, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &AssistantWidget::onPolicyChanged);
}

void AssistantWidget::open() {
  positionPanel();

  setVisible(true);
  raise();
  activateWindow();

  m_open = true;

  if (m_input) {
    m_input->setFocus(Qt::OtherFocusReason);
  }
}

void AssistantWidget::close() {
  setVisible(false);
  m_open = false;
}

void AssistantWidget::toggle() {
  if (m_open) {
    close();
  } else {
    open();
  }
}

void AssistantWidget::dockTo(QWidget *parent) { Q_UNUSED(parent); }

void AssistantWidget::undock() {}

QScreen *AssistantWidget::screenForCurrentPosition() const {
  if (m_drag != DragKind::None) {
    const QPoint cursor = QCursor::pos();

    if (QScreen *screen = QGuiApplication::screenAt(cursor)) {
      return screen;
    }
  }

  const QRect frame = frameGeometry();

  QScreen *best = nullptr;
  qint64 bestArea = -1;

  for (QScreen *screen : QGuiApplication::screens()) {
    const QRect inter = screen->availableGeometry().intersected(frame);
    const qint64 area = static_cast<qint64>(inter.width()) * inter.height();

    if (area > bestArea) {
      bestArea = area;
      best = screen;
    }
  }

  if (best) {
    return best;
  }

  if (QScreen *screen = QGuiApplication::screenAt(frame.center())) {
    return screen;
  }

  return QGuiApplication::primaryScreen();
}

QRect AssistantWidget::currentScreenGeometry() const {
  QScreen *screen = screenForCurrentPosition();

  if (!screen) {
    return QRect(0, 0, 1920, 1080);
  }

  return screen->availableGeometry();
}

void AssistantWidget::positionPanel() {
  const QRect screen = currentScreenGeometry();

  const int maxW = qMax(kPanelMinWidth, screen.width() - 2 * kPanelMargin);
  const int maxH = qMax(kPanelMinHeight, screen.height() - 2 * kPanelMargin);

  setMaximumSize(maxW, maxH);

  const int defaultHeight =
      qBound(kPanelMinHeight,
             (screen.height() * kPanelDefaultHeightFractionNum) /
                 kPanelDefaultHeightFractionDen,
             maxH);

  const int defaultWidth = qMin(kPanelDefaultWidth, maxW);

  QSettings settings;

  if (settings.contains(kPanelPosKey)) {
    const QPoint saved = settings.value(kPanelPosKey).toPoint();

    if (size().isEmpty() || size().width() < 100) {
      resize(defaultWidth, defaultHeight);
    } else {
      resize(qMin(width(), maxW), qMin(height(), maxH));
    }

    const int maxX = qMax(screen.left(), screen.right() - width());
    const int maxY = qMax(screen.top(), screen.bottom() - height());

    move(qBound(screen.left(), saved.x(), maxX),
         qBound(screen.top(), saved.y(), maxY));
    return;
  }

  resize(defaultWidth, defaultHeight);

  move(screen.right() - defaultWidth - kPanelMargin,
       screen.top() + kPanelMargin);
}

void AssistantWidget::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);
}

void AssistantWidget::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  if (!m_card) {
    return;
  }

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const QRect cardRect = m_card->geometry();

  QColor card = m_tokens.surface0.isValid() ? m_tokens.surface0
                                            : palette().color(QPalette::Base);
  card.setAlpha(kCardAlpha);

  QColor border = m_tokens.border.isValid() ? m_tokens.border
                                            : palette().color(QPalette::Mid);
  border.setAlpha(220);

  QPainterPath path;
  const QRectF r = QRectF(cardRect).adjusted(0.5, 0.5, -0.5, -0.5);
  path.addRoundedRect(r, kCardRadius, kCardRadius);

  painter.setPen(QPen(border, 1.0));
  painter.setBrush(card);
  painter.drawPath(path);
}

void AssistantWidget::keyPressEvent(QKeyEvent *event) {
  if (event->key() == Qt::Key_Escape) {
    close();
    event->accept();
    return;
  }

  QWidget::keyPressEvent(event);
}

AssistantWidget::DragKind
AssistantWidget::bandFor(const QPoint &localPos) const {
  const int w = width();
  const int h = height();

  const bool left = localPos.x() < kCornerBand;
  const bool right = localPos.x() > w - kCornerBand;
  const bool top = localPos.y() < kCornerBand;
  const bool bottom = localPos.y() > h - kCornerBand;

  if (left && top) {
    return DragKind::ResizeTopLeft;
  }
  if (right && top) {
    return DragKind::ResizeTopRight;
  }
  if (left && bottom) {
    return DragKind::ResizeBottomLeft;
  }
  if (right && bottom) {
    return DragKind::ResizeBottomRight;
  }

  if (localPos.x() < kEdgeBand) {
    return DragKind::ResizeLeft;
  }
  if (localPos.x() > w - kEdgeBand) {
    return DragKind::ResizeRight;
  }
  if (localPos.y() < kEdgeBand) {
    return DragKind::ResizeTop;
  }
  if (localPos.y() > h - kEdgeBand) {
    return DragKind::ResizeBottom;
  }

  return DragKind::Move;
}

bool AssistantWidget::isDragPoint(const QPoint &pos) const {
  const DragKind kind = bandFor(pos);

  if (kind != DragKind::Move) {
    return true;
  }

  if (!m_header) {
    return false;
  }

  const QPoint headerPos = m_header->mapFrom(this, pos);

  if (!m_header->rect().contains(headerPos)) {
    return false;
  }

  if (m_close && m_close->isVisible()) {
    const QPoint closePos = m_close->mapFrom(this, pos);
    if (m_close->rect().contains(closePos)) {
      return false;
    }
  }

  if (m_policy && m_policy->isVisible()) {
    const QPoint policyPos = m_policy->mapFrom(this, pos);
    if (m_policy->rect().contains(policyPos)) {
      return false;
    }
  }

  return true;
}

void AssistantWidget::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }

  const QPoint local = event->pos();
  const DragKind kind = bandFor(local);

  if (kind == DragKind::Move && !isDragPoint(local)) {
    QWidget::mousePressEvent(event);
    return;
  }

  m_drag = kind;
  m_dragOriginGlobal = event->globalPosition().toPoint();
  m_originGeometry = geometry();

  event->accept();
}

void AssistantWidget::mouseMoveEvent(QMouseEvent *event) {
  if (m_drag == DragKind::None) {
    switch (bandFor(event->pos())) {
    case DragKind::ResizeTopLeft:
    case DragKind::ResizeBottomRight:
      setCursor(Qt::SizeFDiagCursor);
      break;
    case DragKind::ResizeTopRight:
    case DragKind::ResizeBottomLeft:
      setCursor(Qt::SizeBDiagCursor);
      break;
    case DragKind::ResizeLeft:
    case DragKind::ResizeRight:
      setCursor(Qt::SizeHorCursor);
      break;
    case DragKind::ResizeTop:
    case DragKind::ResizeBottom:
      setCursor(Qt::SizeVerCursor);
      break;
    case DragKind::Move:
      if (isDragPoint(event->pos())) {
        setCursor(Qt::SizeAllCursor);
      } else {
        unsetCursor();
      }
      break;
    default:
      unsetCursor();
      break;
    }

    QWidget::mouseMoveEvent(event);
    return;
  }

  const QPoint delta =
      event->globalPosition().toPoint() - m_dragOriginGlobal;

  if (m_drag == DragKind::Move) {
    move(m_originGeometry.topLeft() + delta);
    event->accept();
    return;
  }

  applyResize(delta);
  event->accept();
}

void AssistantWidget::mouseReleaseEvent(QMouseEvent *event) {
  if (m_drag == DragKind::None) {
    QWidget::mouseReleaseEvent(event);
    return;
  }

  m_drag = DragKind::None;

  savePosition();

  emit positionChanged();

  event->accept();
}

void AssistantWidget::applyResize(const QPoint &globalDelta) {
  const QRect screen = currentScreenGeometry();

  const int maxW = qMax(kPanelMinWidth, screen.width() - 2 * kPanelMargin);
  const int maxH = qMax(kPanelMinHeight, screen.height() - 2 * kPanelMargin);

  setMaximumSize(maxW, maxH);

  QRect target = m_originGeometry;

  const int dx = globalDelta.x();
  const int dy = globalDelta.y();

  switch (m_drag) {
  case DragKind::ResizeTopLeft:
    target.setTopLeft(target.topLeft() + QPoint(dx, dy));
    break;
  case DragKind::ResizeTopRight:
    target.setTopRight(target.topRight() + QPoint(dx, dy));
    break;
  case DragKind::ResizeBottomLeft:
    target.setBottomLeft(target.bottomLeft() + QPoint(dx, dy));
    break;
  case DragKind::ResizeBottomRight:
    target.setBottomRight(target.bottomRight() + QPoint(dx, dy));
    break;
  case DragKind::ResizeLeft:
    target.setLeft(target.left() + dx);
    break;
  case DragKind::ResizeRight:
    target.setRight(target.right() + dx);
    break;
  case DragKind::ResizeTop:
    target.setTop(target.top() + dy);
    break;
  case DragKind::ResizeBottom:
    target.setBottom(target.bottom() + dy);
    break;
  default:
    return;
  }

  if (target.width() > maxW) {
    if (m_drag == DragKind::ResizeLeft ||
        m_drag == DragKind::ResizeTopLeft ||
        m_drag == DragKind::ResizeBottomLeft) {
      target.setLeft(target.right() - maxW + 1);
    } else {
      target.setRight(target.left() + maxW - 1);
    }
  }

  if (target.height() > maxH) {
    if (m_drag == DragKind::ResizeTop ||
        m_drag == DragKind::ResizeTopLeft ||
        m_drag == DragKind::ResizeTopRight) {
      target.setTop(target.bottom() - maxH + 1);
    } else {
      target.setBottom(target.top() + maxH - 1);
    }
  }

  if (target.width() < kPanelMinWidth) {
    if (m_drag == DragKind::ResizeLeft ||
        m_drag == DragKind::ResizeTopLeft ||
        m_drag == DragKind::ResizeBottomLeft) {
      target.setLeft(target.right() - kPanelMinWidth + 1);
    } else {
      target.setRight(target.left() + kPanelMinWidth - 1);
    }
  }

  if (target.height() < kPanelMinHeight) {
    if (m_drag == DragKind::ResizeTop ||
        m_drag == DragKind::ResizeTopLeft ||
        m_drag == DragKind::ResizeTopRight) {
      target.setTop(target.bottom() - kPanelMinHeight + 1);
    } else {
      target.setBottom(target.top() + kPanelMinHeight - 1);
    }
  }

  if (target.left() < screen.left()) {
    target.setLeft(screen.left());
  }
  if (target.top() < screen.top()) {
    target.setTop(screen.top());
  }
  if (target.right() > screen.right()) {
    target.setRight(screen.right());
  }
  if (target.bottom() > screen.bottom()) {
    target.setBottom(screen.bottom());
  }

  setGeometry(target);
}

void AssistantWidget::restoreSavedPosition() { positionPanel(); }

void AssistantWidget::savePosition() const {
  QSettings settings;
  settings.setValue(kPanelPosKey, pos());
}

void AssistantWidget::onSubmit() {
  if (m_busy) {
    return;
  }

  const QString text = m_input->text().trimmed();

  if (text.isEmpty()) {
    return;
  }

  m_input->clear();

  emit messageSubmitted(text);
}

void AssistantWidget::onDictateClicked() {
  if (!m_speech) {
    return;
  }

  if (m_speech->isCapturing()) {
    m_speech->endCapture();
    return;
  }

  m_speech->beginCapture();
}

void AssistantWidget::onLiveDictateClicked() {
  if (!m_speech) {
    return;
  }

  if (m_speech->isLiveCapturing() || m_speech->isLiveStarting()) {
    m_speech->stopLiveCapture();
    return;
  }

  m_speech->startLiveCapture();
}

void AssistantWidget::onReadAloudClicked() {
  if (!m_speech || !m_assistant) {
    return;
  }

  const QString last = m_assistant->lastReply();

  if (last.isEmpty()) {
    return;
  }

  m_speech->speakText(last);
}

void AssistantWidget::onCloseClicked() { close(); }

void AssistantWidget::onPolicyChanged(int index) {
  if (!m_assistant || !m_policy) {
    return;
  }

  const int value = m_policy->itemData(index).toInt();

  const auto policy =
      static_cast<LoreAssistant::CompletionPolicy>(value);

  m_assistant->setCompletionPolicy(policy);

  QSettings settings;
  settings.setValue(kPolicyKey, value);
}

void AssistantWidget::applySpeechButtonState(QToolButton *button,
                                             bool active) {
  if (!button) {
    return;
  }

  button->setProperty("active", active);
  button->style()->unpolish(button);
  button->style()->polish(button);
}

void AssistantWidget::onSpeechStateChanged() {
  if (!m_speech) {
    return;
  }

  applySpeechButtonState(m_dictate, m_speech->isCapturing());
  applySpeechButtonState(
      m_live, m_speech->isLiveCapturing() || m_speech->isLiveStarting());
  applySpeechButtonState(m_readAloud, m_speech->isSpeaking());
}

void AssistantWidget::onTranscribed(const QString &text) {
  if (text.trimmed().isEmpty()) {
    return;
  }

  if (m_input) {
    m_input->setText(text);
  }

  onSubmit();
}

void AssistantWidget::onLiveTranscribed(const QString &text, bool isFinal) {
  if (!m_input || text.isEmpty()) {
    return;
  }

  m_input->setText(text);
  m_input->setCursorPosition(text.length());

  if (isFinal) {
    onSubmit();
  }
}

void AssistantWidget::renderTranscript() {
  if (!m_transcript) {
    return;
  }

  QString markdown;

  for (const Entry &entry : m_entries) {
    switch (entry.kind) {
    case Entry::Kind::User:
      markdown += QStringLiteral("**You:** ");
      markdown += entry.text;
      markdown += QStringLiteral("\n\n");
      break;
    case Entry::Kind::Assistant:
      markdown += entry.text;
      markdown += QStringLiteral("\n\n");
      break;
    case Entry::Kind::Status:
      markdown += QStringLiteral("*");
      markdown += entry.text;
      markdown += QStringLiteral("*\n\n");
      break;
    }
  }

  const int scroll = m_transcript->verticalScrollBar()->value();

  m_transcript->document()->setMarkdown(
      markdown, QTextDocument::MarkdownDialectGitHub);

  m_transcript->verticalScrollBar()->setValue(scroll);
  m_transcript->ensureCursorVisible();
}

void AssistantWidget::appendUserMessage(const QString &text) {
  Entry entry;
  entry.kind = Entry::Kind::User;
  entry.text = text;
  m_entries.append(entry);
  renderTranscript();
}

void AssistantWidget::appendAssistantChunk(const QString &text) {
  if (!m_entries.isEmpty() &&
      m_entries.last().kind == Entry::Kind::Assistant) {
    m_entries.last().text += text;
  } else {
    Entry entry;
    entry.kind = Entry::Kind::Assistant;
    entry.text = text;
    m_entries.append(entry);
  }

  renderTranscript();
}

void AssistantWidget::appendStatusMessage(const QString &text) {
  Entry entry;
  entry.kind = Entry::Kind::Status;
  entry.text = text;
  m_entries.append(entry);
  renderTranscript();
}

void AssistantWidget::clearTranscript() {
  m_entries.clear();
  renderTranscript();
}

void AssistantWidget::setBusy(bool busy) {
  m_busy = busy;

  if (m_input) {
    m_input->setEnabled(!busy);
  }

  if (m_send) {
    m_send->setEnabled(!busy);
  }

  if (m_dictate) {
    m_dictate->setEnabled(!busy);
  }

  if (m_live) {
    m_live->setEnabled(!busy);
  }
}