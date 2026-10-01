#pragma once

#include <QIcon>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <functional>

class QAction;
class QMenu;
class QMenuBar;
class QToolButton;

class CustomTitleBar : public QWidget {
  Q_OBJECT

public:
  struct Callbacks {
    std::function<void()> newText;
    std::function<void()> newMarkdown;
    std::function<void()> newPlantUml;
    std::function<void()> newDot;
    std::function<void()> newMermaid;
    std::function<void()> newHtml;
    std::function<void()> open;
    std::function<void()> importFiles;
    std::function<void()> importFolder;
    std::function<void()> save;
    std::function<void()> saveAll;
    std::function<void()> exit;
    std::function<void()> modeNormal;
    std::function<void()> modeOverseer;
    std::function<void()> modeSearch;
    std::function<void()> toggleSpeech;
    std::function<void()> talkToLore;
    std::function<void()> openSettings;
    std::function<void()> openLlmSettings;
    std::function<void()> manageModels;
    std::function<void()> rebuildIndex;
    std::function<void(const QString &)> selectNormalTheme;
    std::function<void(const QString &)> selectOverseerTheme;
    std::function<void()> about;
    std::function<void()> aboutQt;
  };

  explicit CustomTitleBar(QWidget *parent = nullptr);

  void setCallbacks(const Callbacks &callbacks);

  void setNormalThemes(const QStringList &names, const QString &current);
  void setOverseerThemes(const QStringList &names, const QString &current);

  void setModeText(const QString &text);
  void setModeIcon(const QIcon &icon);
  void setModeChecked(int modeIndex);

signals:
  void minimizeRequested();
  void maximizeRequested();
  void closeRequested();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;

private:
  void buildMenus();
  void buildToolbarArea();

  bool isDragRegion(const QPoint &pos) const;
  void toggleMaximizeRestore();
  void updateMaximizeIcon();

  Callbacks m_callbacks;

  QMenuBar *m_menuBar = nullptr;
  QMenu *m_fileMenu = nullptr;
  QMenu *m_viewMenu = nullptr;
  QMenu *m_toolsMenu = nullptr;
  QMenu *m_themeMenu = nullptr;
  QMenu *m_helpMenu = nullptr;
  QMenu *m_normalThemeMenu = nullptr;
  QMenu *m_overseerThemeMenu = nullptr;

  QAction *m_normalModeAct = nullptr;
  QAction *m_overseerModeAct = nullptr;
  QAction *m_searchModeAct = nullptr;

  QToolButton *m_modeButton = nullptr;
  QMenu *m_modeMenu = nullptr;
  QToolButton *m_loreButton = nullptr;

  QToolButton *m_minimizeBtn = nullptr;
  QToolButton *m_maximizeBtn = nullptr;
  QToolButton *m_closeBtn = nullptr;

  bool m_dragging = false;
  QPoint m_dragOrigin;
};