#include "FileManagerWidget.hpp"

#include <QApplication>
#include <QDateTime>
#include <QDialog>
#include <QDirIterator>
#include <QHBoxLayout>
#include <QImageReader>
#include <QInputDialog>
#include <QMessageBox>
#include <QScrollArea>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QStyle>
#include <QVBoxLayout>
#include <QtConcurrent>
#include <algorithm>

namespace directdrop {

static bool isImageSuffix(const QString& ext) {
    static const QSet<QString> kExts = {
        QStringLiteral("jpg"), QStringLiteral("jpeg"), QStringLiteral("png"),
        QStringLiteral("webp"), QStringLiteral("gif"), QStringLiteral("bmp"),
        QStringLiteral("svg"), QStringLiteral("tiff"), QStringLiteral("ico")
    };
    return kExts.contains(ext.toLower());
}

FileManagerWidget::FileManagerWidget(QWidget* parent) : QWidget(parent) {
    m_dirWatcher = new QFutureWatcher<QList<FileEntryMeta>>(this);
    connect(m_dirWatcher, &QFutureWatcher<QList<FileEntryMeta>>::finished, this, &FileManagerWidget::onDirectoryLoaded);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(10);

    // 1. Top Segmented Tab Bar (Images | Files | Folders) + Volumes
    auto* tabRow = new QHBoxLayout();
    m_tabImages = new QPushButton(QStringLiteral("Images"), this);
    m_tabFiles = new QPushButton(QStringLiteral("Files"), this);
    m_tabFolders = new QPushButton(QStringLiteral("Folders"), this);
    for (auto* b : {m_tabImages, m_tabFiles, m_tabFolders}) {
        b->setCheckable(true);
        b->setMinimumHeight(34);
    }
    m_tabFiles->setChecked(true);

    m_volumesCombo = new QComboBox(this);
    m_volumesCombo->addItem(QStringLiteral("Home"), QDir::homePath());
    QString dl = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    if (!dl.isEmpty()) m_volumesCombo->addItem(QStringLiteral("Downloads"), dl);
    QString pics = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (!pics.isEmpty()) m_volumesCombo->addItem(QStringLiteral("Pictures"), pics);
    for (const auto& vol : QStorageInfo::mountedVolumes()) {
        if (vol.isValid() && vol.isReady() && !vol.isReadOnly()) {
            QString name = vol.displayName().isEmpty() ? vol.rootPath() : vol.displayName();
            if (vol.rootPath() != QStringLiteral("/")) {
                m_volumesCombo->addItem(QStringLiteral("Drive: %1").arg(name), vol.rootPath());
            }
        }
    }

    tabRow->addWidget(m_tabImages);
    tabRow->addWidget(m_tabFiles);
    tabRow->addWidget(m_tabFolders);
    tabRow->addStretch();
    tabRow->addWidget(m_volumesCombo);
    root->addLayout(tabRow);

    // 2. Location + Search + View Controls
    auto* navRow = new QHBoxLayout();
    m_btnUp = new QPushButton(QStringLiteral("↑ Up"), this);
    m_lblPath = new QLabel(m_currentDir.absolutePath(), this);
    m_lblPath->setWordWrap(false);
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(QStringLiteral("Search in current view..."));
    m_sortCombo = new QComboBox(this);
    m_sortCombo->addItems({QStringLiteral("Sort: Name"), QStringLiteral("Sort: Size"), QStringLiteral("Sort: Type"), QStringLiteral("Sort: Date")});
    m_filterCombo = new QComboBox(this);
    m_filterCombo->addItems({QStringLiteral("All"), QStringLiteral("Images"), QStringLiteral("Videos"), QStringLiteral("Documents"), QStringLiteral("Archives")});
    m_btnGrid = new QPushButton(QStringLiteral("Grid"), this);
    m_btnHidden = new QPushButton(QStringLiteral("Hidden"), this);
    m_btnHidden->setCheckable(true);
    m_btnRefresh = new QPushButton(QStringLiteral("Refresh"), this);

    navRow->addWidget(m_btnUp);
    navRow->addWidget(m_lblPath, 1);
    navRow->addWidget(m_searchEdit, 1);
    navRow->addWidget(m_sortCombo);
    navRow->addWidget(m_filterCombo);
    navRow->addWidget(m_btnGrid);
    navRow->addWidget(m_btnHidden);
    navRow->addWidget(m_btnRefresh);
    root->addLayout(navRow);

    // 3. Main File / Image / Folder List or Grid
    m_list = new QListWidget(this);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_list->setUniformItemSizes(false);
    root->addWidget(m_list, 1);

    m_lblState = new QLabel(this);
    m_lblState->setStyleSheet(QStringLiteral("color: rgba(255,255,255,0.68); font-size: 12px;"));
    root->addWidget(m_lblState);

    // 4. Local Ops + Transfer Queue Strip
    auto* actionRow = new QHBoxLayout();
    m_btnSelectAll = new QPushButton(QStringLiteral("Select All"), this);
    m_btnClearSel = new QPushButton(QStringLiteral("Clear"), this);
    m_btnPreview = new QPushButton(QStringLiteral("Preview"), this);
    m_btnNewFolder = new QPushButton(QStringLiteral("New Folder"), this);
    m_btnRename = new QPushButton(QStringLiteral("Rename"), this);
    m_btnDelete = new QPushButton(QStringLiteral("Delete"), this);
    m_lblQueue = new QLabel(QStringLiteral("Queue: 0"), this);
    m_btnAddToQueue = new QPushButton(QStringLiteral("Add to Queue"), this);
    m_btnSendQueue = new QPushButton(QStringLiteral("Send Queued →"), this);

    actionRow->addWidget(m_btnSelectAll);
    actionRow->addWidget(m_btnClearSel);
    actionRow->addWidget(m_btnPreview);
    actionRow->addWidget(m_btnNewFolder);
    actionRow->addWidget(m_btnRename);
    actionRow->addWidget(m_btnDelete);
    actionRow->addStretch();
    actionRow->addWidget(m_lblQueue);
    actionRow->addWidget(m_btnAddToQueue);
    actionRow->addWidget(m_btnSendQueue);
    root->addLayout(actionRow);

    connect(m_tabImages, &QPushButton::clicked, this, [this]() { setTab(Tab::Images); });
    connect(m_tabFiles, &QPushButton::clicked, this, [this]() { setTab(Tab::Files); });
    connect(m_tabFolders, &QPushButton::clicked, this, [this]() { setTab(Tab::Folders); });
    connect(m_volumesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        QString p = m_volumesCombo->itemData(idx).toString();
        if (!p.isEmpty()) navigateTo(p);
    });
    connect(m_btnUp, &QPushButton::clicked, this, &FileManagerWidget::onUpClicked);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &FileManagerWidget::onSearchChanged);
    connect(m_sortCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FileManagerWidget::onSortChanged);
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &FileManagerWidget::onFilterChanged);
    connect(m_btnGrid, &QPushButton::clicked, this, &FileManagerWidget::onToggleGrid);
    connect(m_btnHidden, &QPushButton::clicked, this, &FileManagerWidget::onToggleHidden);
    connect(m_btnRefresh, &QPushButton::clicked, this, &FileManagerWidget::onRefresh);
    connect(m_list, &QListWidget::itemDoubleClicked, this, &FileManagerWidget::onItemActivated);
    connect(m_list, &QListWidget::itemChanged, this, &FileManagerWidget::onItemChanged);
    connect(m_btnSelectAll, &QPushButton::clicked, this, [this]() { selectAllItems(true); });
    connect(m_btnClearSel, &QPushButton::clicked, this, [this]() { selectAllItems(false); });
    connect(m_btnPreview, &QPushButton::clicked, this, &FileManagerWidget::onPreviewSelected);
    connect(m_btnNewFolder, &QPushButton::clicked, this, &FileManagerWidget::onCreateFolder);
    connect(m_btnRename, &QPushButton::clicked, this, &FileManagerWidget::onRenameSelected);
    connect(m_btnDelete, &QPushButton::clicked, this, &FileManagerWidget::onDeleteSelected);
    connect(m_btnAddToQueue, &QPushButton::clicked, this, &FileManagerWidget::addSelectedToQueue);
    connect(m_btnSendQueue, &QPushButton::clicked, this, [this]() {
        if (m_queuedPaths.isEmpty()) addSelectedToQueue();
        if (!m_queuedPaths.isEmpty()) emit sendRequested(m_queuedPaths, m_queuedBaseDir);
    });

    startLoad();
}

FileManagerWidget::~FileManagerWidget() {
    m_loadGeneration.fetch_add(1);
    if (m_dirWatcher && m_dirWatcher->isRunning()) {
        m_dirWatcher->waitForFinished();
    }
}

void FileManagerWidget::setTab(Tab t) {
    m_tab = t;
    m_tabImages->setChecked(t == Tab::Images);
    m_tabFiles->setChecked(t == Tab::Files);
    m_tabFolders->setChecked(t == Tab::Folders);
    if (t == Tab::Images) {
        setGridMode(true);
    }
    startLoad();
}

void FileManagerWidget::navigateTo(const QString& dirPath) {
    QDir d(dirPath);
    if (!d.exists()) {
        m_lblState->setText(QStringLiteral("Directory unavailable: %1").arg(dirPath));
        return;
    }
    m_currentDir = d;
    m_lblPath->setText(m_currentDir.absolutePath());
    startLoad();
}

void FileManagerWidget::setSearchText(const QString& text) {
    m_searchText = text.trimmed();
    if (m_searchEdit->text() != text) m_searchEdit->setText(text);
    renderEntries();
}

void FileManagerWidget::setSortMode(int mode) {
    m_sortMode = qBound(0, mode, 3);
    if (m_sortCombo->currentIndex() != m_sortMode) m_sortCombo->setCurrentIndex(m_sortMode);
    renderEntries();
}

void FileManagerWidget::setGridMode(bool grid) {
    m_gridMode = grid;
    m_btnGrid->setText(grid ? QStringLiteral("List") : QStringLiteral("Grid"));
    m_list->setViewMode(grid ? QListView::IconMode : QListView::ListMode);
    m_list->setIconSize(grid ? QSize(88, 88) : QSize(28, 28));
    m_list->setGridSize(grid ? QSize(132, 132) : QSize());
    m_list->setResizeMode(QListView::Adjust);
    m_list->setWordWrap(grid);
    renderEntries();
}

void FileManagerWidget::setShowHidden(bool show) {
    m_showHidden = show;
    m_btnHidden->setChecked(show);
    startLoad();
}

void FileManagerWidget::selectAllItems(bool select) {
    QSignalBlocker blocker(m_list);
    for (int i = 0; i < m_list->count(); ++i) {
        auto* it = m_list->item(i);
        QString path = it->data(Qt::UserRole).toString();
        it->setCheckState(select ? Qt::Checked : Qt::Unchecked);
        if (select) m_selectedPaths.insert(path);
        else m_selectedPaths.remove(path);
    }
    m_lblState->setText(QStringLiteral("%1 item(s) shown • %2 selected").arg(m_list->count()).arg(m_selectedPaths.size()));
}

void FileManagerWidget::addSelectedToQueue() {
    for (const QString& p : m_selectedPaths) {
        QFileInfo fi(p);
        if (!fi.exists()) continue;
        if (fi.isDir()) {
            QDirIterator it(p, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) {
                QString child = it.next();
                if (!m_queuedPaths.contains(child)) m_queuedPaths.append(child);
            }
            if (m_queuedBaseDir.isEmpty()) m_queuedBaseDir = fi.absolutePath();
        } else if (fi.isFile()) {
            if (!m_queuedPaths.contains(p)) m_queuedPaths.append(p);
        }
    }
    updateQueueSummary();
}

void FileManagerWidget::updateQueueSummary() {
    qint64 total = 0;
    for (const QString& p : m_queuedPaths) total += QFileInfo(p).size();
    m_lblQueue->setText(QStringLiteral("Queue: %1 (%2 MB)").arg(m_queuedPaths.size()).arg(total / (1024.0 * 1024.0), 0, 'f', 2));
    emit queueChanged(m_queuedPaths.size(), total);
}

void FileManagerWidget::onSearchChanged(const QString& text) { setSearchText(text); }
void FileManagerWidget::onSortChanged(int idx) { setSortMode(idx); }
void FileManagerWidget::onFilterChanged(int idx) {
    m_filterKey = m_filterCombo->itemText(idx);
    renderEntries();
}
void FileManagerWidget::onToggleGrid() { setGridMode(!m_gridMode); }
void FileManagerWidget::onToggleHidden() { setShowHidden(m_btnHidden->isChecked()); }
void FileManagerWidget::onRefresh() { startLoad(); }
void FileManagerWidget::onUpClicked() {
    QDir d = m_currentDir;
    if (d.cdUp()) navigateTo(d.absolutePath());
}

void FileManagerWidget::onItemActivated(QListWidgetItem* item) {
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    QFileInfo fi(path);
    if (fi.isDir()) {
        navigateTo(path);
    } else {
        onPreviewSelected();
    }
}

void FileManagerWidget::onItemChanged(QListWidgetItem* item) {
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    if (item->checkState() == Qt::Checked) m_selectedPaths.insert(path);
    else m_selectedPaths.remove(path);
    m_lblState->setText(QStringLiteral("%1 item(s) shown • %2 selected").arg(m_list->count()).arg(m_selectedPaths.size()));
}

void FileManagerWidget::onPreviewSelected() {
    auto* item = m_list->currentItem();
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    QFileInfo fi(path);
    if (!fi.exists() || fi.isDir()) return;

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Preview — %1").arg(fi.fileName()));
    dlg.resize(540, 420);
    auto* lay = new QVBoxLayout(&dlg);
    if (isImageSuffix(fi.suffix())) {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        QImage img = reader.read();
        auto* lbl = new QLabel(&dlg);
        lbl->setAlignment(Qt::AlignCenter);
        if (!img.isNull()) {
            lbl->setPixmap(QPixmap::fromImage(img).scaled(500, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else {
            lbl->setText(QStringLiteral("Unsupported or unreadable image format."));
        }
        lay->addWidget(lbl, 1);
    } else {
        QFile f(path);
        QString text;
        if (f.open(QIODevice::ReadOnly)) {
            QByteArray bytes = f.read(16384);
            text = bytes.contains('\0') ? QStringLiteral("Binary file (%1 bytes)").arg(fi.size()) : QString::fromUtf8(bytes);
        } else {
            text = QStringLiteral("Cannot open file: %1").arg(f.errorString());
        }
        auto* lbl = new QLabel(text, &dlg);
        lbl->setWordWrap(true);
        auto* scroll = new QScrollArea(&dlg);
        scroll->setWidgetResizable(true);
        scroll->setWidget(lbl);
        lay->addWidget(scroll, 1);
    }
    dlg.exec();
}

void FileManagerWidget::onCreateFolder() {
    bool ok = false;
    QString name = QInputDialog::getText(this, QStringLiteral("New Folder"), QStringLiteral("Folder name:"), QLineEdit::Normal, {}, &ok);
    if (!ok || name.trimmed().isEmpty()) return;
    if (!m_currentDir.mkdir(name.trimmed())) {
        QMessageBox::warning(this, QStringLiteral("Create Folder Failed"), QStringLiteral("Could not create '%1'.").arg(name));
        return;
    }
    startLoad();
}

void FileManagerWidget::onRenameSelected() {
    auto* item = m_list->currentItem();
    if (!item) return;
    QString path = item->data(Qt::UserRole).toString();
    QFileInfo fi(path);
    bool ok = false;
    QString newName = QInputDialog::getText(this, QStringLiteral("Rename"), QStringLiteral("New name:"), QLineEdit::Normal, fi.fileName(), &ok);
    if (!ok || newName.trimmed().isEmpty() || newName == fi.fileName()) return;
    QString target = fi.dir().filePath(newName.trimmed());
    if (!QFile::rename(path, target)) {
        QMessageBox::warning(this, QStringLiteral("Rename Failed"), QStringLiteral("Could not rename '%1'.").arg(fi.fileName()));
        return;
    }
    m_selectedPaths.remove(path);
    startLoad();
}

void FileManagerWidget::onDeleteSelected() {
    if (m_selectedPaths.isEmpty()) return;
    auto res = QMessageBox::question(
        this,
        QStringLiteral("Confirm Local Delete"),
        QStringLiteral("Permanently delete %1 selected local item(s) from disk? (Sending never deletes originals.)").arg(m_selectedPaths.size()),
        QMessageBox::Yes | QMessageBox::No
    );
    if (res != QMessageBox::Yes) return;
    for (const QString& p : m_selectedPaths) {
        QFileInfo fi(p);
        if (!fi.exists() || fi.isSymLink()) continue;
        if (fi.isDir()) QDir(p).removeRecursively();
        else QFile::remove(p);
    }
    m_selectedPaths.clear();
    startLoad();
}

void FileManagerWidget::startLoad() {
    const uint64_t gen = m_loadGeneration.fetch_add(1) + 1;
    QString dirPath = m_currentDir.absolutePath();
    Tab tab = m_tab;
    bool showHidden = m_showHidden;
    m_lblState->setText(QStringLiteral("Loading..."));

    auto future = QtConcurrent::run([gen, dirPath, tab, showHidden]() -> QList<FileEntryMeta> {
        Q_UNUSED(gen);
        QList<FileEntryMeta> out;
        QDir d(dirPath);
        QDir::Filters filters = QDir::NoDotAndDotDot | (showHidden ? QDir::Hidden : QDir::Filters());
        if (tab == Tab::Folders) {
            filters |= QDir::Dirs;
        } else if (tab == Tab::Images) {
            filters |= QDir::Files | QDir::Dirs;
        } else {
            filters |= QDir::AllEntries;
        }

        const auto list = d.entryInfoList(filters, QDir::DirsFirst | QDir::Name);
        for (const QFileInfo& fi : list) {
            if (tab == Tab::Images && !fi.isDir() && !isImageSuffix(fi.suffix())) continue;
            if (tab == Tab::Folders && !fi.isDir()) continue;
            FileEntryMeta m;
            m.path = fi.absoluteFilePath();
            m.name = fi.fileName();
            m.suffix = fi.suffix().toLower();
            m.size = fi.isDir() ? 0 : fi.size();
            m.modifiedMs = fi.lastModified().toMSecsSinceEpoch();
            m.isDir = fi.isDir();
            out.append(m);
        }
        return out;
    });
    m_dirWatcher->setFuture(future);
}

void FileManagerWidget::onDirectoryLoaded() {
    m_allEntries = m_dirWatcher->result();
    renderEntries();
}

bool FileManagerWidget::matchesFilter(const FileEntryMeta& m) const {
    if (!m_searchText.isEmpty() && !m.name.contains(m_searchText, Qt::CaseInsensitive)) return false;
    if (m.isDir || m_filterKey == QStringLiteral("All")) return true;
    if (m_filterKey == QStringLiteral("Images")) return isImageSuffix(m.suffix);
    if (m_filterKey == QStringLiteral("Videos")) return QStringList{QStringLiteral("mp4"), QStringLiteral("mkv"), QStringLiteral("mov"), QStringLiteral("avi")}.contains(m.suffix);
    if (m_filterKey == QStringLiteral("Documents")) return QStringList{QStringLiteral("pdf"), QStringLiteral("txt"), QStringLiteral("md"), QStringLiteral("doc"), QStringLiteral("docx")}.contains(m.suffix);
    if (m_filterKey == QStringLiteral("Archives")) return QStringList{QStringLiteral("zip"), QStringLiteral("tar"), QStringLiteral("gz"), QStringLiteral("7z")}.contains(m.suffix);
    return true;
}

void FileManagerWidget::renderEntries() {
    QList<FileEntryMeta> visible;
    for (const auto& m : m_allEntries) {
        if (matchesFilter(m)) visible.append(m);
    }
    std::sort(visible.begin(), visible.end(), [this](const FileEntryMeta& a, const FileEntryMeta& b) {
        if (a.isDir != b.isDir) return a.isDir;
        if (m_sortMode == 1) return a.size > b.size;
        if (m_sortMode == 2) return a.suffix < b.suffix;
        if (m_sortMode == 3) return a.modifiedMs > b.modifiedMs;
        return a.name.toLower() < b.name.toLower();
    });

    QSignalBlocker blocker(m_list);
    m_list->clear();
    QIcon dirIcon = QApplication::style()->standardIcon(QStyle::SP_DirIcon);
    QIcon fileIcon = QApplication::style()->standardIcon(QStyle::SP_FileIcon);

    for (const auto& m : visible) {
        QString label = m.isDir
            ? m.name
            : QStringLiteral("%1 (%2 MB)").arg(m.name).arg(m.size / (1024.0 * 1024.0), 0, 'f', 2);
        auto* item = new QListWidgetItem(m.isDir ? dirIcon : fileIcon, label, m_list);
        item->setData(Qt::UserRole, m.path);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(m_selectedPaths.contains(m.path) ? Qt::Checked : Qt::Unchecked);

        if (!m.isDir && isImageSuffix(m.suffix)) {
            if (auto* cached = m_thumbCache.object(m.path)) {
                item->setIcon(QIcon(*cached));
            } else {
                requestThumbnail(m.path, item);
            }
        }
    }
    m_lblState->setText(visible.isEmpty()
        ? QStringLiteral("No matching items in %1").arg(m_currentDir.dirName())
        : QStringLiteral("%1 item(s) shown • %2 selected").arg(visible.size()).arg(m_selectedPaths.size()));
}

void FileManagerWidget::requestThumbnail(const QString& path, QListWidgetItem*) {
    if (m_pendingThumbs.contains(path)) return;
    m_pendingThumbs.insert(path);
    const uint64_t gen = m_loadGeneration.load();
    auto future = QtConcurrent::run([path]() -> QImage {
        QImageReader reader(path);
        reader.setAutoTransform(true);
        reader.setScaledSize(QSize(96, 96));
        return reader.read();
    });
    auto* watcher = new QFutureWatcher<QImage>(this);
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, path, gen]() {
        m_pendingThumbs.remove(path);
        QImage img = watcher->result();
        watcher->deleteLater();
        if (gen == m_loadGeneration.load() && !img.isNull()) {
            onThumbReady(path, img);
        }
    });
    watcher->setFuture(future);
}

void FileManagerWidget::onThumbReady(const QString& path, const QImage& img) {
    auto* pix = new QPixmap(QPixmap::fromImage(img));
    m_thumbCache.insert(path, pix);
    for (int i = 0; i < m_list->count(); ++i) {
        auto* it = m_list->item(i);
        if (it->data(Qt::UserRole).toString() == path) {
            it->setIcon(QIcon(*pix));
            break;
        }
    }
}

} // namespace directdrop
