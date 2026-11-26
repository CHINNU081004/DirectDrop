#pragma once

#include <QCache>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QWidget>
#include <atomic>

namespace directdrop {

struct FileEntryMeta {
    QString path;
    QString name;
    QString suffix;
    qint64 size = 0;
    qint64 modifiedMs = 0;
    bool isDir = false;
    int childCount = 0;
};

class FileManagerWidget : public QWidget {
    Q_OBJECT
public:
    enum class Tab { Images = 0, Files = 1, Folders = 2 };

    explicit FileManagerWidget(QWidget* parent = nullptr);
    ~FileManagerWidget() override;

    Tab currentTab() const { return m_tab; }
    QString currentDirectory() const { return m_currentDir.absolutePath(); }
    QStringList queuedFiles() const { return m_queuedPaths; }
    QString queuedBaseDir() const { return m_queuedBaseDir; }
    bool isGridMode() const { return m_gridMode; }
    int itemCount() const { return m_list->count(); }

    // Programmatic helpers (also used by unit tests)
    void setTab(Tab t);
    void navigateTo(const QString& dirPath);
    void setSearchText(const QString& text);
    void setSortMode(int mode); // 0=Name, 1=Size, 2=Type, 3=Date
    void setGridMode(bool grid);
    void setShowHidden(bool show);
    void selectAllItems(bool select);
    void addSelectedToQueue();

signals:
    void queueChanged(int count, qint64 totalBytes);
    void sendRequested(const QStringList& files, const QString& baseDir);
    void statusMessage(const QString& msg);

private slots:
    void onSearchChanged(const QString& text);
    void onSortChanged(int idx);
    void onFilterChanged(int idx);
    void onToggleGrid();
    void onToggleHidden();
    void onRefresh();
    void onUpClicked();
    void onItemActivated(QListWidgetItem* item);
    void onItemChanged(QListWidgetItem* item);
    void onPreviewSelected();
    void onCreateFolder();
    void onRenameSelected();
    void onDeleteSelected();
    void onDirectoryLoaded();
    void onThumbReady(const QString& path, const QImage& img);

private:
    void startLoad();
    void renderEntries();
    void requestThumbnail(const QString& path, QListWidgetItem* item);
    void updateQueueSummary();
    bool matchesFilter(const FileEntryMeta& m) const;

    Tab m_tab = Tab::Files;
    QDir m_currentDir{QDir::homePath()};
    bool m_gridMode = false;
    bool m_showHidden = false;
    int m_sortMode = 0;
    QString m_searchText;
    QString m_filterKey = QStringLiteral("All");

    QList<FileEntryMeta> m_allEntries;
    QSet<QString> m_selectedPaths;
    QStringList m_queuedPaths;
    QString m_queuedBaseDir;

    QCache<QString, QPixmap> m_thumbCache{250};
    QSet<QString> m_pendingThumbs;
    std::atomic_uint64_t m_loadGeneration{0};
    QFutureWatcher<QList<FileEntryMeta>>* m_dirWatcher = nullptr;

    QPushButton* m_tabImages = nullptr;
    QPushButton* m_tabFiles = nullptr;
    QPushButton* m_tabFolders = nullptr;
    QPushButton* m_btnUp = nullptr;
    QPushButton* m_btnGrid = nullptr;
    QPushButton* m_btnHidden = nullptr;
    QPushButton* m_btnRefresh = nullptr;
    QPushButton* m_btnSelectAll = nullptr;
    QPushButton* m_btnClearSel = nullptr;
    QPushButton* m_btnPreview = nullptr;
    QPushButton* m_btnNewFolder = nullptr;
    QPushButton* m_btnRename = nullptr;
    QPushButton* m_btnDelete = nullptr;
    QPushButton* m_btnAddToQueue = nullptr;
    QPushButton* m_btnSendQueue = nullptr;

    QComboBox* m_volumesCombo = nullptr;
    QComboBox* m_sortCombo = nullptr;
    QComboBox* m_filterCombo = nullptr;
    QLineEdit* m_searchEdit = nullptr;
    QLabel* m_lblPath = nullptr;
    QLabel* m_lblState = nullptr;
    QLabel* m_lblQueue = nullptr;
    QListWidget* m_list = nullptr;
};

} // namespace directdrop
