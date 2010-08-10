// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Interfaces_UiWidgetProperties_h
#define incl_Interfaces_UiWidgetProperties_h

#include "UiDefines.h"

#include <QObject>
#include <QSize>
#include <QIcon>
#include <QPointF>
#include <QString>
#include <QMap>

namespace UiServices
{
    //! Enumeration of different widget types supported by UiModule.
    enum WidgetType
    {
        /*! Only to be used internally with CoreUi widgets in UiModule 
         *  *QWidget without frames by default 
         *  *Animations off by default
         */
        CoreLayoutWidget,

        /*! Module Widget, button is added to main panel to control show/hide 
         *  *QDialog with frames by default 
         *  *Animations on by default
         */
        ModuleWidget,

        /*! Normal Scene Widget, button not added to main panel. Just shown in the scene. 
         *  *QDialog with frames by default 
         *  *Animations on by default
         */
        SceneWidget
    };

    class UiWidgetProperties : public QObject
    {
        Q_OBJECT
        // READ
        Q_PROPERTY(WidgetType widget_type_ READ GetWidgetType)
        //read-write now for the workaround in py for constructors not working there yet
        //Q_PROPERTY(QString widget_name_ READ GetWidgetName)
        Q_PROPERTY(Qt::WindowFlags window_type_ READ GetWindowStyle)
        Q_PROPERTY(bool show_in_toolbar_ READ IsShownInToolbar)
        // READ & WRITE
        Q_PROPERTY(QPointF position_ READ GetPosition WRITE SetPosition)
        Q_PROPERTY(QString widget_name_ READ GetWidgetName WRITE SetWidgetName)
        Q_ENUMS(WidgetType)

    public:
        /*! Creates new properties according to the widget_type. For types check the enum documentation in the header.
         *  Default type is ModuleWidget, will have window frames and a control button is added to the main panel for show/hide
         *  \param QString widget_name, name of the widget
         *  \param UiServices::WidgetType widget_type, type of the window
         */
        UiWidgetProperties(const QString &name, WidgetType type, QIcon icon = QIcon()) :
            widget_name_(name),
            widget_type_(type),
            position_(QPointF(10.0, 200.0)),
            widgets_icon_(icon),
            menu_group_(UiDefines::NoGroup)
        {
            switch (widget_type_)
            {
            case CoreLayoutWidget:
                window_type_ = Qt::Widget;
                show_in_toolbar_ = false;
                break;
            case ModuleWidget:
                window_type_ = Qt::Dialog;
                show_in_toolbar_ = true;
                menu_group_ = UiDefines::RootGroup;
                break;
            case SceneWidget:
                window_type_ = Qt::Dialog;
                show_in_toolbar_ = false;
                break;
            }
        }

        //! Copy-constructor.
        UiWidgetProperties(const UiWidgetProperties &rhs) :
            widget_type_(rhs.GetWidgetType()),
            widget_name_(rhs.GetWidgetName()),
            window_type_(rhs.GetWindowStyle()),
            show_in_toolbar_(rhs.IsShownInToolbar()),
            position_(rhs.GetPosition()),
            menu_image_map_(rhs.GetMenuNodeStyleMap()),
            menu_group_(rhs.GetMenuGroup())
        {
        }

        //! Destructor.
        ~UiWidgetProperties() {}

    public slots:
        //! Getters for properties
        WidgetType GetWidgetType() const { return widget_type_; }
        const QString GetWidgetName() const { return widget_name_; }
        const Qt::WindowFlags GetWindowStyle() const { return window_type_; }
        const QPointF GetPosition() const { return position_; }
        bool IsShownInToolbar() const { return show_in_toolbar_; }
        QIcon GetWidgetIcon() const { return widgets_icon_; }
        UiDefines::MenuGroup GetMenuGroup() const { return menu_group_; }
        UiDefines::MenuNodeStyleMap GetMenuNodeStyleMap() const { return menu_image_map_; }

        //! Setters for properties
        void SetPosition(const QPointF &position) { position_ = position; }
        void SetWidgetName(const QString &newname) { widget_name_ = newname; }
        void SetMenuGroup(UiDefines::MenuGroup menu_group) { menu_group_ = menu_group; }
        void SetMenuGroup(int menu_group) { menu_group_ = (UiDefines::MenuGroup)menu_group; }
        void SetMenuNodeStyleMap(UiDefines::MenuNodeStyleMap menu_style_to_image_path) { menu_image_map_ = menu_style_to_image_path; }

        // For python, cant call with the whole enum map, individual setters
        void SetMenuNodeIconNormal(const QString &path) { menu_image_map_[UiDefines::IconNormal] = path; }
        void SetMenuNodeIconHover(const QString &path) { menu_image_map_[UiDefines::IconHover] = path; }
        void SetMenuNodeIconPressed(const QString &path) { menu_image_map_[UiDefines::IconPressed] = path; }

    private:
        WidgetType widget_type_;
        Qt::WindowFlags window_type_;
        QString widget_name_;
        QPointF position_;
        bool show_in_toolbar_;
        QIcon widgets_icon_;

        UiDefines::MenuGroup menu_group_; 
        UiDefines::MenuNodeStyleMap menu_image_map_;
    };
}

#endif // incl_UiModule_UiWidgetProperties_h