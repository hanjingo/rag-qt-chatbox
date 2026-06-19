#ifndef PLUGININTERFACE_H
#define PLUGININTERFACE_H

#include <QObject>
#include <QString>
#include <QWidget>

#include "Bus.h"

class PluginInterface
{
  public:
    virtual ~PluginInterface()                   = default;
    virtual QString  Id()                        = 0;
    virtual QString  Name()                      = 0;
    virtual QString  Icon()                      = 0;
    virtual QString  Version()                   = 0;
    virtual QWidget *Init(Bus *parent = nullptr) = 0;
};

QT_BEGIN_NAMESPACE
#define PluginInterface_iid "rag-qt.PluginInterface"
Q_DECLARE_INTERFACE(PluginInterface, PluginInterface_iid)
QT_END_NAMESPACE

#endif // PLUGININTERFACE_H