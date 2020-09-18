/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtQml module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include <private/qqmlglobal_p.h>
#include <QtQml/private/qqmlmetatype_p.h>

#include <QtQml/qqmlengine.h>
#include <QtCore/qvariant.h>
#include <QtCore/qstringlist.h>
#include <QtCore/qdebug.h>
#include <QtCore/QCoreApplication>

QT_BEGIN_NAMESPACE

QQmlValueTypeProvider::QQmlValueTypeProvider()
    : next(nullptr)
{
}

QQmlValueTypeProvider::~QQmlValueTypeProvider()
{
    QQml_removeValueTypeProvider(this);
}

bool QQmlValueTypeProvider::initValueType(int type, QVariant& dst)
{
    const QMetaType metaType(type);
    if (!metaType.isValid())
        return false;
    dst = QVariant(QMetaType(type));
    return true;
}

QVariant QQmlValueTypeProvider::createValueType(int type, const QJSValue &params)
{
    QVariant v;

    QQmlValueTypeProvider *p = this;
    do {
        if (p->create(type, params, &v))
            return v;
    } while ((p = p->next));

    return QVariant();
}

bool QQmlValueTypeProvider::createValueFromString(int type, const QJSValue &s, QVariant *data)
{
    Q_ASSERT(data);

    QQmlValueTypeProvider *p = this;
    do {
        if (p->create(type, s, data))
            return true;
    } while ((p = p->next));

    return false;
}

QVariant QQmlValueTypeProvider::createVariantFromString(int type, const QString &s, bool *ok)
{
    QVariant v;

    QQmlValueTypeProvider *p = this;
    do {
        if (p->variantFromString(type, s, &v)) {
            if (ok) *ok = true;
            return v;
        }
    } while ((p = p->next));

    if (ok) *ok = false;
    return QVariant();
}

QVariant QQmlValueTypeProvider::createVariantFromJsObject(int type, const QJSValue &obj, bool *ok)
{
    QVariant v;

    QQmlValueTypeProvider *p = this;
    do {
        if (p->create(type, obj, &v)) {
            if (ok) *ok = true;
            return v;
        }
    } while ((p = p->next));

    if (ok) *ok = false;
    return QVariant();
}

bool QQmlValueTypeProvider::equalValueType(int type, const void *lhs, const QVariant& rhs)
{
    Q_ASSERT(lhs);
    return QMetaType(type).equals(lhs, rhs.constData());
}

bool QQmlValueTypeProvider::storeValueType(int type, const void *src, void *dst, size_t dstSize)
{
    Q_ASSERT(src);
    Q_ASSERT(dst);

    QQmlValueTypeProvider *p = this;
    do {
        if (p->store(type, src, dst, dstSize))
            return true;
    } while ((p = p->next));

    return false;
}

bool QQmlValueTypeProvider::readValueType(const QVariant& src, void *dst, int type)
{
    Q_ASSERT(dst);
    const QMetaType dstType(type);
    if (!dstType.isValid() || (src.metaType() == dstType && dstType.equals(src.constData(), dst)))
        return false;

    dstType.destruct(dst);
    dstType.construct(dst, src.metaType() == dstType ? src.constData() : nullptr);
    return true;
}

bool QQmlValueTypeProvider::writeValueType(int type, const void *src, QVariant& dst)
{
    Q_ASSERT(src);
    const QMetaType srcType(type);
    if (!srcType.isValid() || (dst.metaType() == srcType && srcType.equals(src, dst.constData())))
        return false;

    dst = QVariant(srcType, src);
    return true;
}

bool QQmlValueTypeProvider::create(int, const QJSValue &, QVariant *) { return false; }
bool QQmlValueTypeProvider::variantFromString(int, const QString &, QVariant *) { return false; }
bool QQmlValueTypeProvider::store(int, const void *, void *, size_t) { return false; }

struct ValueTypeProviderList {
    QQmlValueTypeProvider nullProvider;
    QQmlValueTypeProvider *head = &nullProvider;
};

Q_GLOBAL_STATIC(ValueTypeProviderList, valueTypeProviders)

Q_QML_PRIVATE_EXPORT void QQml_addValueTypeProvider(QQmlValueTypeProvider *newProvider)
{
    if (ValueTypeProviderList *providers = valueTypeProviders()) {
        newProvider->next = providers->head;
        providers->head = newProvider;
    }
}

Q_QML_PRIVATE_EXPORT void QQml_removeValueTypeProvider(QQmlValueTypeProvider *oldProvider)
{
    if (ValueTypeProviderList *providers = valueTypeProviders()) {
        QQmlValueTypeProvider *prev = providers->head;
        if (prev == oldProvider) {
            providers->head = oldProvider->next;
            return;
        }

        // singly-linked list removal
        for (; prev; prev = prev->next) {
            if (prev->next != oldProvider)
                continue;               // this is not the provider you're looking for
            prev->next = oldProvider->next;
            return;
        }

        qWarning("QQml_removeValueTypeProvider: was asked to remove provider %p but it was not found", oldProvider);
    }
}

Q_AUTOTEST_EXPORT QQmlValueTypeProvider *QQml_valueTypeProvider()
{
    if (ValueTypeProviderList *providers = valueTypeProviders())
        return providers->head;
    return nullptr;
}

QQmlColorProvider::~QQmlColorProvider() {}
QVariant QQmlColorProvider::colorFromString(const QString &, bool *ok) { if (ok) *ok = false; return QVariant(); }
unsigned QQmlColorProvider::rgbaFromString(const QString &, bool *ok) { if (ok) *ok = false; return 0; }
QVariant QQmlColorProvider::fromRgbF(double, double, double, double) { return QVariant(); }
QVariant QQmlColorProvider::fromHslF(double, double, double, double) { return QVariant(); }
QVariant QQmlColorProvider::fromHsvF(double, double, double, double) { return QVariant(); }
QVariant QQmlColorProvider::lighter(const QVariant &, qreal) { return QVariant(); }
QVariant QQmlColorProvider::darker(const QVariant &, qreal) { return QVariant(); }
QVariant QQmlColorProvider::alpha(const QVariant &, qreal)
{
    return QVariant();
}
QVariant QQmlColorProvider::tint(const QVariant &, const QVariant &) { return QVariant(); }

static QQmlColorProvider *colorProvider = nullptr;

Q_QML_PRIVATE_EXPORT QQmlColorProvider *QQml_setColorProvider(QQmlColorProvider *newProvider)
{
    QQmlColorProvider *old = colorProvider;
    colorProvider = newProvider;
    return old;
}

static QQmlColorProvider **getColorProvider(void)
{
    if (colorProvider == nullptr) {
        qWarning() << "Warning: QQml_colorProvider: no color provider has been set!";
        static QQmlColorProvider nullColorProvider;
        colorProvider = &nullColorProvider;
    }

    return &colorProvider;
}

Q_AUTOTEST_EXPORT QQmlColorProvider *QQml_colorProvider(void)
{
    static QQmlColorProvider **providerPtr = getColorProvider();
    return *providerPtr;
}


QQmlGuiProvider::~QQmlGuiProvider() {}
QObject *QQmlGuiProvider::application(QObject *) { return new QQmlApplication(); }
QStringList QQmlGuiProvider::fontFamilies() { return QStringList(); }
bool QQmlGuiProvider::openUrlExternally(QUrl &) { return false; }

QObject *QQmlGuiProvider::inputMethod()
{
    // We don't have any input method code by default
    QObject *o = new QObject();
    o->setObjectName(QStringLiteral("No inputMethod available"));
    QQmlEngine::setObjectOwnership(o, QQmlEngine::JavaScriptOwnership);
    return o;
}

QObject *QQmlGuiProvider::styleHints()
{
    QObject *o = new QObject();
    o->setObjectName(QStringLiteral("No styleHints available"));
    QQmlEngine::setObjectOwnership(o, QQmlEngine::JavaScriptOwnership);
    return o;
}

QString QQmlGuiProvider::pluginName() const { return QString(); }

static QQmlGuiProvider *guiProvider = nullptr;

Q_QML_PRIVATE_EXPORT QQmlGuiProvider *QQml_setGuiProvider(QQmlGuiProvider *newProvider)
{
    QQmlGuiProvider *old = guiProvider;
    guiProvider = newProvider;
    return old;
}

static QQmlGuiProvider **getGuiProvider(void)
{
    if (guiProvider == nullptr) {
        static QQmlGuiProvider nullGuiProvider; //Still provides an application with no GUI support
        guiProvider = &nullGuiProvider;
    }

    return &guiProvider;
}

Q_AUTOTEST_EXPORT QQmlGuiProvider *QQml_guiProvider(void)
{
    static QQmlGuiProvider **providerPtr = getGuiProvider();
    return *providerPtr;
}

//Docs in qqmlengine.cpp
QQmlApplication::QQmlApplication(QObject *parent)
    : QObject(*(new QQmlApplicationPrivate),parent)
{
    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()),
            this, SIGNAL(aboutToQuit()));
    connect(QCoreApplication::instance(), SIGNAL(applicationNameChanged()),
            this, SIGNAL(nameChanged()));
    connect(QCoreApplication::instance(), SIGNAL(applicationVersionChanged()),
            this, SIGNAL(versionChanged()));
    connect(QCoreApplication::instance(), SIGNAL(organizationNameChanged()),
            this, SIGNAL(organizationChanged()));
    connect(QCoreApplication::instance(), SIGNAL(organizationDomainChanged()),
            this, SIGNAL(domainChanged()));
}

QQmlApplication::QQmlApplication(QQmlApplicationPrivate &dd, QObject *parent)
    : QObject(dd, parent)
{
    connect(QCoreApplication::instance(), SIGNAL(aboutToQuit()),
            this, SIGNAL(aboutToQuit()));
    connect(QCoreApplication::instance(), SIGNAL(applicationNameChanged()),
            this, SIGNAL(nameChanged()));
    connect(QCoreApplication::instance(), SIGNAL(applicationVersionChanged()),
            this, SIGNAL(versionChanged()));
    connect(QCoreApplication::instance(), SIGNAL(organizationNameChanged()),
            this, SIGNAL(organizationChanged()));
    connect(QCoreApplication::instance(), SIGNAL(organizationDomainChanged()),
            this, SIGNAL(domainChanged()));
}

QStringList QQmlApplication::args()
{
    Q_D(QQmlApplication);
    if (!d->argsInit) {
        d->argsInit = true;
        d->args = QCoreApplication::arguments();
    }
    return d->args;
}

QString QQmlApplication::name() const
{
    return QCoreApplication::instance()->applicationName();
}

QString QQmlApplication::version() const
{
    return QCoreApplication::instance()->applicationVersion();
}

QString QQmlApplication::organization() const
{
    return QCoreApplication::instance()->organizationName();
}

QString QQmlApplication::domain() const
{
    return QCoreApplication::instance()->organizationDomain();
}

void QQmlApplication::setName(const QString &arg)
{
    QCoreApplication::instance()->setApplicationName(arg);
}

void QQmlApplication::setVersion(const QString &arg)
{
    QCoreApplication::instance()->setApplicationVersion(arg);
}

void QQmlApplication::setOrganization(const QString &arg)
{
    QCoreApplication::instance()->setOrganizationName(arg);
}

void QQmlApplication::setDomain(const QString &arg)
{
    QCoreApplication::instance()->setOrganizationDomain(arg);
}

QT_END_NAMESPACE

#include "moc_qqmlglobal_p.cpp"
