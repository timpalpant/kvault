#pragma once

#include <QObject>
#include <QQmlEngine>

namespace kvault {

/// Generates passwords and passphrases, mirroring Bitwarden's options.
class PasswordGenerator : public QObject
{
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(Mode mode READ mode WRITE setMode NOTIFY optionsChanged)
    Q_PROPERTY(QString value READ value NOTIFY valueChanged)

    // Password options
    Q_PROPERTY(int length READ length WRITE setLength NOTIFY optionsChanged)
    Q_PROPERTY(bool useUppercase READ useUppercase WRITE setUseUppercase NOTIFY optionsChanged)
    Q_PROPERTY(bool useLowercase READ useLowercase WRITE setUseLowercase NOTIFY optionsChanged)
    Q_PROPERTY(bool useDigits READ useDigits WRITE setUseDigits NOTIFY optionsChanged)
    Q_PROPERTY(bool useSpecial READ useSpecial WRITE setUseSpecial NOTIFY optionsChanged)
    Q_PROPERTY(bool avoidAmbiguous READ avoidAmbiguous WRITE setAvoidAmbiguous NOTIFY optionsChanged)
    Q_PROPERTY(int minDigits READ minDigits WRITE setMinDigits NOTIFY optionsChanged)
    Q_PROPERTY(int minSpecial READ minSpecial WRITE setMinSpecial NOTIFY optionsChanged)

    // Passphrase options
    Q_PROPERTY(int wordCount READ wordCount WRITE setWordCount NOTIFY optionsChanged)
    Q_PROPERTY(QString separator READ separator WRITE setSeparator NOTIFY optionsChanged)
    Q_PROPERTY(bool capitalise READ capitalise WRITE setCapitalise NOTIFY optionsChanged)
    Q_PROPERTY(bool includeNumber READ includeNumber WRITE setIncludeNumber NOTIFY optionsChanged)

    /// 0-4, in the style of zxcvbn, for the strength meter.
    Q_PROPERTY(int strength READ strength NOTIFY valueChanged)

public:
    enum Mode {
        Password,
        Passphrase,
    };
    Q_ENUM(Mode)

    explicit PasswordGenerator(QObject *parent = nullptr);

    /// Produce a new value using the current options.
    Q_INVOKABLE void regenerate();

    /// Rough strength estimate of an arbitrary string, for the item editor.
    Q_INVOKABLE static int estimateStrength(const QString &password);

    Mode mode() const { return m_mode; }
    void setMode(Mode mode);
    QString value() const { return m_value; }
    int strength() const;

    int length() const { return m_length; }
    void setLength(int length);
    bool useUppercase() const { return m_useUppercase; }
    void setUseUppercase(bool use);
    bool useLowercase() const { return m_useLowercase; }
    void setUseLowercase(bool use);
    bool useDigits() const { return m_useDigits; }
    void setUseDigits(bool use);
    bool useSpecial() const { return m_useSpecial; }
    void setUseSpecial(bool use);
    bool avoidAmbiguous() const { return m_avoidAmbiguous; }
    void setAvoidAmbiguous(bool avoid);
    int minDigits() const { return m_minDigits; }
    void setMinDigits(int count);
    int minSpecial() const { return m_minSpecial; }
    void setMinSpecial(int count);

    int wordCount() const { return m_wordCount; }
    void setWordCount(int count);
    QString separator() const { return m_separator; }
    void setSeparator(const QString &separator);
    bool capitalise() const { return m_capitalise; }
    void setCapitalise(bool capitalise);
    bool includeNumber() const { return m_includeNumber; }
    void setIncludeNumber(bool include);

Q_SIGNALS:
    void optionsChanged();
    void valueChanged();

private:
    QString generatePassword() const;
    QString generatePassphrase() const;

    Mode m_mode = Password;
    QString m_value;

    int m_length = 16;
    bool m_useUppercase = true;
    bool m_useLowercase = true;
    bool m_useDigits = true;
    bool m_useSpecial = true;
    bool m_avoidAmbiguous = false;
    int m_minDigits = 1;
    int m_minSpecial = 1;

    int m_wordCount = 4;
    QString m_separator = QStringLiteral("-");
    bool m_capitalise = true;
    bool m_includeNumber = true;
};

} // namespace kvault
