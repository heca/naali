// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_ECEditorModule_ECBrowser_h
#define incl_ECEditorModule_ECBrowser_h

#define QT_QTPROPERTYBROWSER_IMPORT

#include <QtTreePropertyBrowser>
#include <map>
#include <set>
#include "CoreTypes.h"
#include "ComponentGroup.h"

#include <boost/shared_ptr.hpp>
#include <boost/weak_ptr.hpp>

class QtTreePropertyBrowser;
class QtGroupPropertyManager;
class QtBrowserItem;
class QTreeWidget;
class QMenu;

namespace Foundation
{
    class Framework;
    class ComponentInterface;
    typedef boost::shared_ptr<ComponentInterface> ComponentInterfacePtr;
    typedef boost::weak_ptr<ComponentInterface> ComponentWeakPtr;
}

namespace Scene
{
    class Entity;
}

namespace ECEditor
{
    class ECComponentEditor;
    typedef std::vector<Foundation::ComponentWeakPtr> ComponentWeakPtrVector;
    typedef std::list<ComponentGroup*> ComponentGroupList;

    //! ECBrowser is a widget that will display all selected entity components and their attributes.
    /*! The ECBrowser will iterate all entity's components and pass them to a ECComponentEditor switch is responsible to handling component's attribute editing.
     *  User can add new editable enitites by using AddEntity and RemoveEntity methods and the browser will iterate trhought all the entitys components and pass them to ECComponentEditors.
     *  ECBrowser has implement options to add, delete, copy and paste components from the selected entities by using CopyComponent, DeleteComponent, PasteComponent and EditXml mehtods.
     *  \todo Try to find a way to remove the unecessary paint events when we are updating the browser parameters.
     *  \ingroup ECEditorModuleClient.
     */
    class ECBrowser: public QtTreePropertyBrowser
    {
        Q_OBJECT
    public:
        ECBrowser(Foundation::Framework *framework, QWidget *parent = 0);
        virtual ~ECBrowser();

        //! Insert new entity to browser and add it's components to the browser.
        void AddNewEntity(Scene::Entity *entity);
        //! Remove edited entity from the browser widget.
        void RemoveEntity(Scene::Entity *entity);

    public slots:
        //! Reset browser state to where it was after the browser initialization. Override method from the QtTreePropertyBrowser.
        void clear();
        //! Update editor data and browser ui elements if needed.
        void UpdateBrowser();

    signals:
        //! User want to open xml editor for that spesific component type.
        void ShowXmlEditorForComponent(const std::string &componentType);
        //! User want to add new component for selected entities.
        void CreateNewComponent();
        //! Emitted when component is selected from the browser widget.
        void ComponentSelected(Foundation::ComponentInterface *component);

    private:
        //! Initialize browser widget and create all connections for different QObjects.
        void InitBrowser();
        //! Try to find the right component group for spesific component type. if found return it's position on the list as in iterator format.
        //! If any component group wasn't found return .end() iterator value.
        ComponentGroupList::iterator FindSuitableGroup(const Foundation::ComponentInterface &comp);
        //! Try to find component group for spesific QTreeWidgetItem.
        ComponentGroupList::iterator FindSuitableGroup(const QTreeWidgetItem &item);
        //! Add new component to existing component group if same type of component have been already added to editor,
        //! if component type is not included as yet to browser create new component group and add it to editor's list container.
        void AddNewComponentToGroup(Foundation::ComponentInterfacePtr comp);
        //! Remove component from registered componentgroup. Do nothing if component was not found of any component groups.
        void RemoveComponentFromGroup(Foundation::ComponentInterface *comp);
        //! Remove whole component group object from the browser.
        void RemoveComponentGroup(ComponentGroup *componentGroup);

    private slots:
        //! User have right clicked the browser and QMenu need to be open to display copy, paste, delete ations etc.
        void ShowComponentContextMenu(const QPoint &pos);
        //! QTreeWidget has changed it's focus and we need to highlight new entities from the editor window.
        void SelectionChanged();
        //! a new component have been added to entity.
        void NewComponentAdded(Scene::Entity* entity, Foundation::ComponentInterface* comp);
        //! Component have been removed from the entity.
        void ComponentRemoved(Scene::Entity* entity, Foundation::ComponentInterface* comp);
        //! User has selected xml edit action from a QMenu.
        void OpenComponentXmlEditor();
        //! User has selected copy action from a QMenu.
        void CopyComponent();
        //! User has selected paste action from a QMenu.
        void PasteComponent();
        //! User has selected delete action from a QMenu.
        void DeleteComponent();
        //! New dynamic component attribute has been added.
        //! @todo When many attributes has been added/removed from the editor this method is called multiple times and each time this method seems to reinitialize the component editor.
        //! This will consume too much time and should be fixed so that coponent editor will be initialized only once when the dynamic component's attributes changes.
        void DynamicComponentChanged(const QString &name);
        //! Components name has been changed and we need to repostion it on the browser.
        void ComponentNameChanged(const std::string &newName);
        //! Show dialog, so that user can create a new attribute.
        void CreateAttribute();
        //! Remove selected attribute from the dynamic component.
        void RemoveAttribute();

    private:
        ComponentGroupList componentGroups_;
        typedef std::set<Scene::Entity *> EntitySet;
        EntitySet selectedEntities_;
        QMenu *menu_;
        QTreeWidget *treeWidget_;
        Foundation::Framework *framework_;
    };
}

#endif