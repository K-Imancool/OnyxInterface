#ifndef HALFSOCKET_H
#define HALFSOCKET_H

#include <QString>
#include <QStringList>
#include <QSharedPointer>

#include "Structures.h"
#include "surgicalmode.h"

class HalfSocket
{
public:
    HalfSocket(bool isCoag, int state = 1);
    HalfSocket(const HalfSocket& other);
    HalfSocket& operator=(const HalfSocket& other);

public:
    // /*! Перечисление возможных состояний сокета */
    // enum HS_State {HS_DISABLED, /*!< Выключен, активация запрещена */
    //                 HS_ENABLED, /*!< Включен, активация разрешена */
    //                     };

    /**
     * @brief Возвращает внутренний индекс текущего  режима
     * @return индекс
     */
    int modeIndex() const;
    /**
     * @brief Возвращает ID текущего режима
     * @return индекс
     */
    int modeId() const;

    /**
     * @brief Возвращает статус сокета
     * @return тип
     */
    Onyx::HS_State halfSocketState() const;

    /**
     * @brief возвращает название текущего режима
     * @return
     */
    QString modeName() const;

    /**
     * @brief Возвращает список доступных режимов
     * @return
     */
    QMap<int, SurgModePtr> modes() const;

    // /**
    //  * @brief Возвращает список доступных режимов коаг
    //  * @return
    //  */
    // const QStringList& modeNames() const;

    /**
     * @brief устанавливает новый относительный индекс режима коагуляции
     * @param newCoagModeIndex - устанавливаемый относительный индекс
     * @return true - индекс установлен
     * @return false - индекс не установлен
     */
    bool setModeIndex(int newModeIndex);

    bool setModeId(int id);

    /**
     * @brief смена текущего состояния сокета
     * @param newSocketStatus
     */
    void setHalfSocketState(Onyx::HS_State newSocketStatus);

    /**
     * @brief Геттер режима
     * @param modeIndex относительный индекс режима в этом сокете
     * @param isCoag выбор между режимами резания и коагуляции
     * @return Указатель на КОНСТАНТНЫЙ режим
     */
    CSurgModePtr getMode(int modeIndex) const;

    /**
     * @brief Возращает текущую мощность текущего режима коагуляции
     * @return
     */
    int modePower() const;

    /**
     * @brief Устанавливает текущую мощность текущего режима коагуляции
     * @param newCoagModePower - задаваемая мощность
     * @return true - смена мощности режима выполнена успешно
     * @return false - изменение мощности не прошло
     */
    bool setModePower(int newModePower);

    /**
     * @brief Задаеёт перечень достпных режимов резания и порядок их отображения
     * @param newCutModes - хэш новых режимов по их имени
     * @param order - порядок в котором список имён режимов будет выводиться пользователю
     * @todo Возможно, с учётом локализаций, необходимо использовать индекс из БД как ключ
     */
    void setModes(const QMap<int, SurgModePtr > &newModes,
                     const QStringList& order = {""});

    /**
     * @brief имена доступных режимов коагуляции
     * @return
     */
    QStringList modeNames() const;

    /**
     * @brief возвращает указатель на КОНСТАНТНЫЙ текущий режим коагуляции
     * @return
     */
    CSurgModePtr curMode() const;

    /**
     * @brief формарование байт-массива текущих настроек сокета
     * @return
     */
    QByteArray toByteArray();

    QStringList modeNamesIds() const;

    bool setInstrumIndex(int index);
    bool setInstrumId(int id);

    /**
     * @brief количество режимов, не считая заглушку
     * @return
     */
    int modeCount();

private:
    int getModeIndex(const QString& name) const;
    int getModeIndex(int id) const;

    CSurgModePtr m_curMode = nullptr;
    int m_modeIndex = 0;

    QStringList m_modeNames;
    QMap<int, SurgModePtr> m_modes;

    bool m_isCoag;
    int m_state;
};

using CHalfSockPtr = QSharedPointer<const HalfSocket>;
using HalfSockPtr = QSharedPointer<HalfSocket>;

#endif // HALFSOCKET_H
