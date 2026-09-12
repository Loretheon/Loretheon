#ifndef EPISTEME_DIRECTORYEXPLORERSETTINGS_H
#define EPISTEME_DIRECTORYEXPLORERSETTINGS_H

#include <QByteArray>
#include <QList>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <Qt>

class DirectoryExplorerSettings : public QObject {
  Q_OBJECT

public:
  static DirectoryExplorerSettings &instance();

  QString rootDirectory() const;
  void setRootDirectory(const QString &path);

  bool showHidden() const;
  void setShowHidden(bool value);

  QStringList nameFilters() const;
  void setNameFilters(const QStringList &filters);

  QList<int> columnOrder() const;
  void setColumnOrder(const QList<int> &order);

  QList<bool> columnVisibility() const;
  void setColumnVisibility(const QList<bool> &visibility);

  static QList<int> defaultColumnOrder();
  static QList<bool> defaultColumnVisibility();

  bool showExtensionColumn() const;
  void setShowExtensionColumn(bool value);

  bool showSizeColumn() const;
  void setShowSizeColumn(bool value);

  bool showTypeColumn() const;
  void setShowTypeColumn(bool value);

  bool showDateModifiedColumn() const;
  void setShowDateModifiedColumn(bool value);

  bool showDateCreatedColumn() const;
  void setShowDateCreatedColumn(bool value);

  bool headerHidden() const;
  void setHeaderHidden(bool value);

  bool alternatingRowColors() const;
  void setAlternatingRowColors(bool value);

  int indentation() const;
  void setIndentation(int value);

  int iconSize() const;
  void setIconSize(int value);

  QByteArray headerState() const;
  void setHeaderState(const QByteArray &state);

  int sortColumn() const;
  void setSortColumn(int column);

  Qt::SortOrder sortOrder() const;
  void setSortOrder(Qt::SortOrder order);

  QStringList expandedPaths() const;
  void setExpandedPaths(const QStringList &paths);

  QString selectedPath() const;
  void setSelectedPath(const QString &path);

  void sync();

private:
  explicit DirectoryExplorerSettings(QObject *parent = nullptr);
  static QString settingsPath();

  QSettings m_settings;
};

#endif // EPISTEME_DIRECTORYEXPLORERSETTINGS_H