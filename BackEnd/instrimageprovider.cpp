#include "instrimageprovider.h"
#include "apppaths.h"

#include <QDir>
#include <QMutex>

// Статические члены
QFileInfoList InstrImageProvider::s_knownFiles;
bool InstrImageProvider::s_filesScanned = false;

void InstrImageProvider::scanFiles()
{
    static QMutex mutex;
    QMutexLocker locker(&mutex);
    
    if (s_filesScanned)
        return;
    
    QStringList filters;
    filters << "*.png" << "*.PNG";

    ///TODO зарефакторить - полученный ниже подход в случае расширения очень повторяем и несёт
    /// в себе опасность итеративных одинаковых ошибок.
    //получаем список фалов в корне, добавляем его как set в map под строкой, допустим, empty
    //получаем список папок, добавляем их содержимое под названием папки
    //получив имя, проверям его наличие в мапе кэша
    //если нету
    //проверям наличие папки (это символы до первого слеша), так получаем нужный сет, и проверяем в нём
    //если нет - то если сильно хочется(но мне кажется это зло, т.к.errorProne со всех сторон)
    //то ищем проверяем, добивая номерные нули.


    
    const AppPaths &paths = AppPaths::instance();

    // Сканируем папку с инструментами
    QDir instrDir(paths.instrumentsDir());
    if (instrDir.exists()) {
        s_knownFiles = instrDir.entryInfoList(filters, QDir::Files
                                                        | QDir::NoDotAndDotDot
                                                        | QDir::Readable);
        // qDebug() << "InstrImageProvider: Found" << s_knownFiles.size() << "instrument files in" << instrDir.absolutePath();
    } else {
        // qWarning() << "InstrImageProvider: Directory does not exist:" << instrDir.absolutePath();
    }
    
    // Сканируем папку с режимами
    QDir modesDir(paths.modesDir());
    if (modesDir.exists()) {
        QFileInfoList modeFiles = modesDir.entryInfoList(filters,
                                                        QDir::Files
                                                         | QDir::NoDotAndDotDot
                                                         | QDir::Readable);
        s_knownFiles.append(modeFiles);
        // qDebug() << "InstrImageProvider: Found" << modeFiles.size() << "mode files in" << modesDir.absolutePath();
    } else {
        // qWarning() << "InstrImageProvider: Directory does not exist:" << modesDir.absolutePath();
    }
    
    // Сканируем папку со скоупами
    QDir scopesDir(paths.scopesDir());
    if (scopesDir.exists()) {
        QFileInfoList scopeFiles = scopesDir.entryInfoList(filters,
                                                        QDir::Files
                                                         | QDir::NoDotAndDotDot
                                                         | QDir::Readable);
        s_knownFiles.append(scopeFiles);
        // qDebug() << "InstrImageProvider: Found" << scopeFiles.size() << "scope files in" << scopesDir.absolutePath();
    } else {
        // qWarning() << "InstrImageProvider: Directory does not exist:" << scopesDir.absolutePath();
    }
    
    // qDebug() << "InstrImageProvider: Total" << s_knownFiles.size() << "image files loaded";
    
    s_filesScanned = true;
}

InstrImageProvider::InstrImageProvider()
    : QQuickImageProvider(QQuickImageProvider::Pixmap)
{
    scanFiles();
}

QPixmap InstrImageProvider::requestPixmap(const QString &id,
                                            QSize *size,
                                            const QSize &requestedSize)
{
    static QSize defaultSize = QSize(800, 300);
    static double widthToHeight = 160.0/60.0;

    QSize mySize;
    if (requestedSize.width() > 0 && requestedSize.height() > 0) {
        double reqWidthToHeight = double(requestedSize.width()) / double(requestedSize.height());

        if (reqWidthToHeight > widthToHeight)
            mySize = QSize(int(requestedSize.height() * widthToHeight), requestedSize.height());
        else
            mySize = QSize(requestedSize.width(), int(requestedSize.width()/ widthToHeight));

        if (size)
            *size = mySize;
    } else {
        if (size)
            *size = defaultSize;
        mySize = defaultSize;
    }
    
    // Создаём прозрачную заглушку для "пустых" изображений
    static QPixmap emptyPixmap;
    if (emptyPixmap.isNull()) {
        emptyPixmap = QPixmap(1, 1);
        emptyPixmap.fill(Qt::transparent);
    }
    
    // Проверка на отрицательный индекс или пустую строку (не выбран инструмент/режим)
    if (id.contains("-") || id.isEmpty()) {
        if (size)
            *size = mySize;
        return emptyPixmap.scaled(mySize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    
    // Проверка на нулевой индекс (например, monomode0, minstr0, bimode0)
    // или на 1000 (NO_MODE - режим не выбран)
    // Извлекаем число из конца строки
    QString numPart;
    for (int i = id.length() - 1; i >= 0 && id[i].isDigit(); --i) {
        numPart.prepend(id[i]);
    }
    if (!numPart.isEmpty()) {
        int numValue = numPart.toInt();
        if (numValue == 0 || numValue == 1000) {
            if (size)
                *size = mySize;
            return emptyPixmap.scaled(mySize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }
    
    // Проверяем кэш
    if (m_cache.contains(id)) {
        return (m_cache.value(id).scaled(mySize, Qt::KeepAspectRatio));
    }

    // Ищем файл по baseName (сначала точное совпадение)
    for (const auto& item : s_knownFiles) {
        if (item.baseName() == id) {
            QPixmap pixmap(item.absoluteFilePath());
            if (!pixmap.isNull()) {
                m_cache.insert(id, pixmap);
                return pixmap.scaled(mySize, Qt::KeepAspectRatio);
            } else {
                // qWarning() << "InstrImageProvider: Failed to load pixmap from" << item.absoluteFilePath();
        }
    }
    }
    
    // Если не найдено, пробуем с ведущими нулями (для файлов типа instr001.png)
    // Извлекаем префикс и число из id (например, "instr3" -> "instr" + "3")
    QString prefix;
    QString number;
    for (int i = id.length() - 1; i >= 0; --i) {
        if (id[i].isDigit()) {
            number.prepend(id[i]);
        } else {
            prefix = id.left(i + 1);
            break;
        }
    }
    
    if (!number.isEmpty()) {
        // Пробуем с разным количеством ведущих нулей и разным регистром
        QStringList prefixVariants;
        prefixVariants << prefix << prefix.toLower();
        
        for (const QString& prefixVar : prefixVariants) {
            QStringList variants;
            variants << QString("%1%2").arg(prefixVar).arg(number.toInt(), 3, 10, QChar('0')); // 001, 002, ...
            variants << QString("%1%2").arg(prefixVar).arg(number.toInt(), 2, 10, QChar('0')); // 01, 02, ...
            variants << QString("%1%2").arg(prefixVar).arg(number); // 1, 2, ...
            
            for (const QString& variant : variants) {
                for (const auto& item : s_knownFiles) {
                    if (item.baseName() == variant) {
                        QPixmap pixmap(item.absoluteFilePath());
                        if (!pixmap.isNull()) {
                            m_cache.insert(id, pixmap);
                            // qDebug() << "InstrImageProvider: Loaded" << id << "(as" << variant << ") from" << item.absoluteFilePath();
                            return pixmap.scaled(mySize, Qt::KeepAspectRatio);
                        }
                    }
                }
            }
        }
    }
    
    // Если не найдено, возвращаем прозрачный pixmap с предупреждением
    // qWarning() << "InstrImageProvider: Image not found for id:" << id << "(searched among" << s_knownFiles.size() << "files)";
    if (size)
        *size = mySize;
    return emptyPixmap.scaled(mySize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
