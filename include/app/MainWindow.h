#ifndef MAINWINDOW_H
#define MAINWINDOW_H


#include <QHash>
#include <QMainWindow>
#include <QSet>
#include <QString>
#include <QStringList>

#include <memory>

#include "DocumentManager.h"
#include "ThemeManager.h"
#include "inference/InferenceService.h"

class AssistantIcon;
class AssistantWidget;
class DocumentArea;
class TextEdit;
class TextDocument;
class FileWidget;
class ChatWidget;
class EditSession;
class LoreTrigger;
class ModelDialog;
class LlmSettingsPanel;
class OverseerPage;
class ToastStack;
class AvatarWidget;
class LoreAssistant;

class IngestRegistry;
class IngestService;
class NoteWriter;
class NotificationService;

class SpeechController;
class SpeechPanel;
class VoiceCommandRegistry;

class ScopeIndex;
class SearchPage;
class SearchService;

class QAction;
class QActionGroup;
class QEvent;
class QMenu;
class QProgressDialog;
class QResizeEvent;
class QStackedWidget;
class QToolBar;
class QToolButton;

class MainWindow : public QMainWindow {
  Q_OBJECT

public:
  MainWindow();
  ~MainWindow() override;

protected:
  void changeEvent(QEvent *event) override;
  void closeEvent(QCloseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void showEvent(QShowEvent *event) override;
  void moveEvent(QMoveEvent *event) override;
private slots:
  void about();
  void aboutQt();
  void manageModels();
  void openLlmSettings();
  void openSettings();
  void onThemeSelected(const QString &theme);
  void onOverseerThemeSelected(const QString &theme);

  void onModeActionTriggered(QAction *action);

  void onImportRequested(const QString &path);
  void onImportAllRequested(const QStringList &paths);
  void onImportFilesDialog();
  void onImportFolderDialog();

  void onToggleSpeechPanel();
  void onCurrentEditorChangedForSpeech(TextEdit *editor);

  void onSearchRequested();
  void onSearchOpenRequested(const QString &filePath,
                             const QString &scopeId);

  void onAssistantMessageSubmitted(const QString &text);
  void onAssistantIconClicked();
  void onTalkToLoreClicked();

private:
  enum class Mode { Normal = 0, Overseer = 1, Search = 2 };
  bool m_avatarPlaced = false;
  void createActions();
  void createMenus();
  void createToolbar();

  void buildNormalPage();
  void buildOverseerPage();
  void buildSearchLayer();

  // The avatar is created early, so LoreAssistant has a valid avatar
  // pointer, and positioned late, after the window has its final
  // geometry. Doing both in one call either leaves the assistant with
  // a null avatar or places the widget against the wrong window size.
  void createAvatarOverlay();
  void positionAvatarOverlay();

  // The icon is screen-anchored. Called when the icon is shown.
  void positionAssistantIcon();

  void setMode(Mode mode);

  bool loadThemeFromResource(const QString &name);
  bool loadAllThemes();
  QString combinedStylesheet(const QString &themeName) const;

  void applyNormalTheme(const QString &name);
  void applyOverseerTheme(const QString &name);

  QPalette paletteForTokens(const ThemeTokens &tokens) const;

  QSet<QString> modifiedPaths() const;
  void bindCurrentEditor(TextEdit *editor);

  bool confirmDiscardChanges(const QString &areaName);

  void buildIngestLayer();

  void importOne(const QString &sourcePath);

  QString notesRootPath() const;

  void reportImportFailure(const QString &sourcePath, const QString &error);
  void reportImportSummary(int succeeded, int failed, int total);

  QString importDialogFilter() const;
  QStringList collectImportableFilesIn(const QString &folderPath) const;
  QStringList filterImportable(const QStringList &paths) const;

  void startBulkImport(const QStringList &paths);
  void onBulkImportCompleted(quint64 token, bool ok);
  void onBulkImportCancelled();
  void finishBulkImport();

  void buildSpeechLayer();

  QWidget *m_normalPage = nullptr;
  DocumentArea *m_documentArea = nullptr;
  FileWidget *m_fileWidget = nullptr;
  DocumentManager *m_documentManager = nullptr;
  EditSession *m_editSession = nullptr;
  ChatWidget *m_chatWidget = nullptr;

  OverseerPage *m_overseerPage = nullptr;
  SearchPage *m_searchPage = nullptr;

  QStackedWidget *m_centralStack = nullptr;
  QToolBar *m_topToolBar = nullptr;
  QToolButton *m_modeButton = nullptr;

  ToastStack *m_toastStack = nullptr;
  AvatarWidget *m_avatar = nullptr;

  LoreAssistant *m_assistant = nullptr;
  AssistantWidget *m_assistantWidget = nullptr;
  AssistantIcon *m_assistantIcon = nullptr;

  InferenceService *m_inferenceService = nullptr;
  ModelDialog *m_modelDialog = nullptr;
  LlmSettingsPanel *m_llmSettingsPanel = nullptr;

  ThemeManager *m_normalThemeManager = nullptr;
  ThemeManager *m_overseerThemeManager = nullptr;

  QString m_currentNormalTheme;
  QString m_currentOverseerTheme;


  QMenu *m_fileMenu = nullptr;
  QMenu *m_newMenu = nullptr;
  QMenu *m_toolsMenu = nullptr;
  QMenu *m_themeMenu = nullptr;
  QMenu *m_viewMenu = nullptr;
  QMenu *m_modeMenu = nullptr;
  QMenu *m_helpMenu = nullptr;

  QAction *m_newTextAct = nullptr;
  QAction *m_newMarkdownAct = nullptr;
  QAction *m_newPlantUmlAct = nullptr;
  QAction *m_openAct = nullptr;
  QAction *m_importFilesAct = nullptr;
  QAction *m_importFolderAct = nullptr;
  QAction *m_saveAct = nullptr;
  QAction *m_saveAllAct = nullptr;
  QAction *m_exitAct = nullptr;
  QAction *m_manageModelsAct = nullptr;
  QAction *m_llmSettingsAct = nullptr;
  QAction *m_settingsAct = nullptr;

  QActionGroup *m_modeGroup = nullptr;
  QAction *m_normalModeAct = nullptr;
  QAction *m_overseerModeAct = nullptr;
  QAction *m_searchModeAct = nullptr;

  QAction *m_toggleSpeechAct = nullptr;
  QAction *m_rebuildIndexAct = nullptr;
  QAction *m_aboutAct = nullptr;
  QAction *m_aboutQtAct = nullptr;

  QAction *m_talkToLoreAct = nullptr;

  std::unique_ptr<IngestRegistry> m_ingestRegistry;
  std::unique_ptr<NoteWriter> m_noteWriter;
  IngestService *m_ingestService = nullptr;

  NotificationService *m_notificationService = nullptr;

  QProgressDialog *m_importProgress = nullptr;

  int m_bulkImportSucceeded = 0;
  int m_bulkImportFailed = 0;
  int m_bulkImportTotal = 0;
  int m_bulkImportCompleted = 0;
  bool m_bulkImportCancelled = false;
  QList<quint64> m_bulkImportTokens;

  SpeechController *m_speechController = nullptr;
  VoiceCommandRegistry *m_voiceCommands = nullptr;
  SpeechPanel *m_speechPanel = nullptr;
  TextEdit *m_currentSpeechEditor = nullptr;

  std::unique_ptr<ScopeIndex> m_scopeIndex;
  SearchService *m_searchService = nullptr;

  QHash<TextEdit *, LoreTrigger *> m_loreTriggers;
};

#endif // MAINWINDOW_H