#ifndef STMUPDATER_H
#define STMUPDATER_H

#include <QObject>
#include <QFileInfo>
#include <QStringList>

#include "linkstm.h"

class StmUpdater : public QObject
{
    Q_OBJECT
public:
    explicit StmUpdater(QObject *parent = nullptr,
                        QFileInfo *fileInfo = nullptr);
    ~StmUpdater();
    QList<LinkStm::HexString> getHexList();

signals:
    void updateError(const QString &errorMsg);


private:
    QStringList m_hexStrList;

    enum class TxHexResultType {
        TxHexOk = 0,
        TxHexErr
    };
    enum class HexRecordType {
        HexData = 0,
        HexEndOfFile,
        HexExtendSegAddress,
        HexStartSegAddress,
        HexExtendLinearAddress,
        HexStartLinearAddress,
        HexError,
        HexErrorCrc,
        HexErrorSym,
        HexErrorType,
    };

private slots:
    HexRecordType parseHexString(const QString &hexStr, QByteArray *data, uint16_t *addrOffset);
};

#endif // STMUPDATER_H
