#include "../../include/file/DirectoryExplorerSettings.h"

#include <QDir>
#include <QStandardPaths>

DirectoryExplorerSettings &DirectoryExplorerSettings::instance() {
  static DirectoryExplorerSettings instance;
  return instance;
}

QString DirectoryExplorerSettings::settingsPath() {
  const QString dir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  QDir().mkpath(dir);
  return dir + "/directory_explorer.ini";
}

DirectoryExplorerSettings::DirectoryExplorerSettings(QObject *parent)
    : QObject(parent), m_settings(settingsPath(), QSettings::IniFormat) {}

QString DirectoryExplorerSettings::rootDirectory() const {
  return m_settings.value("rootDirectory").toString();
}

void DirectoryExplorerSettings::setRootDirectory(const QString &path) {
  m_settings.setValue("rootDirectory", path);
  m_settings.sync();
}

bool DirectoryExplorerSettings::showHidden() const {
  return m_settings.value("showHidden", false).toBool();
}

void DirectoryExplorerSettings::setShowHidden(bool value) {
  m_settings.setValue("showHidden", value);
  m_settings.sync();
}

QStringList DirectoryExplorerSettings::nameFilters() const {
  return m_settings.value("nameFilters").toStringList();
}

void DirectoryExplorerSettings::setNameFilters(const QStringList &filters) {
  m_settings.setValue("nameFilters", filters);
  m_settings.sync();
}

QList<int> DirectoryExplorerSettings::columnOrder() const {
  const QStringList stored = m_settings.value("columnOrder").toStringList();
  QList<int> order;
  for (const QString &s : stored) {
    bool ok = false;
    const int col = s.toInt(&ok);
    if (ok)
      order << col;
  }
  if (order.isEmpty())
    order = defaultColumnOrder();
  return order;
}

void DirectoryExplorerSettings::setColumnOrder(const QList<int> &order) {
  QStringList stored;
  for (int col : order)
    stored << QString::number(col);
  m_settings.setValue("columnOrder", stored);
  m_settings.sync();
}

QList<bool> DirectoryExplorerSettings::columnVisibility() const {
  const QStringList stored = m_settings.value("columnVisibility").toStringList();
  QList<bool> vis;
  for (const QString &s : stored)
    vis << (s == "1");
  if (vis.size() < 6) {
    vis = defaultColumnVisibility();
    while (vis.size() < 6)
      vis << true;
  }
  return vis;
}

void DirectoryExplorerSettings::setColumnVisibility(const QList<bool> &visibility) {
  QStringList stored;
  for (bool v : visibility)
    stored << (v ? "1" : "0");
  m_settings.setValue("columnVisibility", stored);
  m_settings.sync();
}

QList<int> DirectoryExplorerSettings::defaultColumnOrder() {
  return {0, 1, 2, 3, 4, 5};
}

QList<bool> DirectoryExplorerSettings::defaultColumnVisibility() {
  return {true, true, true, true, true, true};
}

bool DirectoryExplorerSettings::showExtensionColumn() const {
  const QList<bool> vis = columnVisibility();
  return vis.size() > 1 ? vis.at(1) : true;
}

void DirectoryExplorerSettings::setShowExtensionColumn(bool value) {
  QList<bool> vis = columnVisibility();
  while (vis.size() < 6)
    vis << true;
  vis[1] = value;
  setColumnVisibility(vis);
}

bool DirectoryExplorerSettings::showSizeColumn() const {
  const QList<bool> vis = columnVisibility();
  return vis.size() > 2 ? vis.at(2) : true;
}

void DirectoryExplorerSettings::setShowSizeColumn(bool value) {
  QList<bool> vis = columnVisibility();
  while (vis.size() < 6)
    vis << true;
  vis[2] = value;
  setColumnVisibility(vis);
}

bool DirectoryExplorerSettings::showTypeColumn() const {
  const QList<bool> vis = columnVisibility();
  return vis.size() > 3 ? vis.at(3) : true;
}

void DirectoryExplorerSettings::setShowTypeColumn(bool value) {
  QList<bool> vis = columnVisibility();
  while (vis.size() < 6)
    vis << true;
  vis[3] = value;
  setColumnVisibility(vis);
}

bool DirectoryExplorerSettings::showDateModifiedColumn() const {
  const QList<bool> vis = columnVisibility();
  return vis.size() > 4 ? vis.at(4) : true;
}

void DirectoryExplorerSettings::setShowDateModifiedColumn(bool value) {
  QList<bool> vis = columnVisibility();
  while (vis.size() < 6)
    vis << true;
  vis[4] = value;
  setColumnVisibility(vis);
}

bool DirectoryExplorerSettings::showDateCreatedColumn() const {
  const QList<bool> vis = columnVisibility();
  return vis.size() > 5 ? vis.at(5) : true;
}

void DirectoryExplorerSettings::setShowDateCreatedColumn(bool value) {
  QList<bool> vis = columnVisibility();
  while (vis.size() < 6)
    vis << true;
  vis[5] = value;
  setColumnVisibility(vis);
}

bool DirectoryExplorerSettings::headerHidden() const {
  return m_settings.value("headerHidden", false).toBool();
}

void DirectoryExplorerSettings::setHeaderHidden(bool value) {
  m_settings.setValue("headerHidden", value);
  m_settings.sync();
}

bool DirectoryExplorerSettings::alternatingRowColors() const {
  return m_settings.value("alternatingRowColors", true).toBool();
}

void DirectoryExplorerSettings::setAlternatingRowColors(bool value) {
  m_settings.setValue("alternatingRowColors", value);
  m_settings.sync();
}

int DirectoryExplorerSettings::indentation() const {
  return m_settings.value("indentation", 18).toInt();
}

void DirectoryExplorerSettings::setIndentation(int value) {
  m_settings.setValue("indentation", value);
  m_settings.sync();
}

int DirectoryExplorerSettings::iconSize() const {
  return m_settings.value("iconSize", 16).toInt();
}

void DirectoryExplorerSettings::setIconSize(int value) {
  m_settings.setValue("iconSize", value);
  m_settings.sync();
}

QByteArray DirectoryExplorerSettings::headerState() const {
  return m_settings.value("headerState").toByteArray();
}

void DirectoryExplorerSettings::setHeaderState(const QByteArray &state) {
  m_settings.setValue("headerState", state);
  m_settings.sync();
}

int DirectoryExplorerSettings::sortColumn() const {
  return m_settings.value("sortColumn", 0).toInt();
}

void DirectoryExplorerSettings::setSortColumn(int column) {
  m_settings.setValue("sortColumn", column);
  m_settings.sync();
}

Qt::SortOrder DirectoryExplorerSettings::sortOrder() const {
  const int value =
      m_settings.value("sortOrder", static_cast<int>(Qt::AscendingOrder)).toInt();
  return value == static_cast<int>(Qt::DescendingOrder) ? Qt::DescendingOrder
                                                        : Qt::AscendingOrder;
}

void DirectoryExplorerSettings::setSortOrder(Qt::SortOrder order) {
  m_settings.setValue("sortOrder", static_cast<int>(order));
  m_settings.sync();
}

QStringList DirectoryExplorerSettings::expandedPaths() const {
  return m_settings.value("expandedPaths").toStringList();
}

void DirectoryExplorerSettings::setExpandedPaths(const QStringList &paths) {
  m_settings.setValue("expandedPaths", paths);
  m_settings.sync();
}

QString DirectoryExplorerSettings::selectedPath() const {
  return m_settings.value("selectedPath").toString();
}

void DirectoryExplorerSettings::setSelectedPath(const QString &path) {
  m_settings.setValue("selectedPath", path);
  m_settings.sync();
}

void DirectoryExplorerSettings::sync() {
  m_settings.sync();
}