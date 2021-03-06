// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "DebugOperatorNew.h"

#include "Application.h"
#include "Framework.h"
#include "VersionInfo.h"
#include "ConfigAPI.h"
#include "Profiler.h"
#include "CoreStringUtils.h"
#include "CoreException.h"
#include "LoggingFunctions.h"

#include <boost/filesystem.hpp>
#include <iostream>
#include <utility>

#include <QDir>
#include <QGraphicsView>
#include <QTranslator>
#include <QLocale>
#include <QIcon>
#include <QWebSettings>
#include <QSplashScreen>

#ifdef Q_WS_MAC
#include <QMouseEvent>
#include <QWheelEvent>

#include "UiMainWindow.h"
#include "UiAPI.h"
#include "UiGraphicsView.h"
#endif

#if defined(_WINDOWS)
#include <WinSock2.h>
#include <windows.h>
#include <shlobj.h>
#undef min
#undef max
#endif

#include "MemoryLeakCheck.h"

Application::Application(Framework *framework_, int &argc, char **argv) :
    QApplication(argc, argv),
    framework(framework_),
    appActivated(true),
    nativeTranslator(new QTranslator),
    appTranslator(new QTranslator),
    splashScreen(0)
{
    QApplication::setApplicationName("Tundra");

    // Make sure that the required Tundra data directories exist.
    boost::filesystem::wpath path(QStringToWString(UserDataDirectory()));
    if (!boost::filesystem::exists(path))
        boost::filesystem::create_directory(path);

    path = boost::filesystem::wpath(QStringToWString(UserDocumentsDirectory()));
    if (!boost::filesystem::exists(path))
        boost::filesystem::create_directory(path);

    // Add <install_dir>/qtplugins for qt to search plugins
    QString runDirectory = InstallationDirectory() + "/qtplugins";
    runDirectory.replace('\\', '/');
    addLibraryPath(runDirectory);

    // In headless mode, we create windows that are never shown.
    // Also, the user can open up debugging windows like the profiler or kNet network stats from the console,
    // so disable the whole application from closing when these are shut down.
    // For headful mode, we depend on explicitly checking on the closing of the main window, so we don't need
    // this flag in any case.
    setQuitOnLastWindowClosed(false);

    QDir dir("data/translations/qt_native_translations");
    QStringList qmFiles = GetQmFiles(dir);

    // Search then that is there corresponding native translations for system locals.
    QString loc = QLocale::system().name();
    loc.chop(3);

    QString name = "data/translations/qt_native_translations/qt_" + loc + ".qm";
    QStringList lst = qmFiles.filter(name);
    if (!lst.empty() )
        nativeTranslator->load(lst[0]);

    this->installTranslator(nativeTranslator);

    if (!framework_->Config()->HasValue(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_FRAMEWORK, "language"))
        framework_->Config()->Set(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_FRAMEWORK, "language", "data/translations/naali_en");

    QString default_language = framework_->Config()->Get(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_FRAMEWORK, "language").toString();
    ChangeLanguage(default_language);

    QWebSettings::globalSettings()->setAttribute(QWebSettings::PluginsEnabled, true); //enablig flash

    InitializeSplash();
}

Application::~Application()
{
    SAFE_DELETE(splashScreen);
    SAFE_DELETE(nativeTranslator);
    SAFE_DELETE(appTranslator);
}

void Application::InitializeSplash()
{
// Don't show splash screen in debug mode as it 
// can obstruct your view if debugging the startup routines.
#ifdef ENABLE_SPLASH_SCREEN
    if (framework->IsHeadless())
        return;

    if (!splashScreen)
    {
        QString runDir = InstallationDirectory();
        splashScreen = new QSplashScreen(QPixmap(runDir + "/data/ui/images/realxtend_tundra_splash.png"));
        splashScreen->setFont(QFont("Calibri", 9));
        splashScreen->show();
        SetSplashMessage("Initializing framework...");
    }
#endif
}

void Application::SetSplashMessage(const QString &message)
{
    if (framework->IsHeadless())
        return;

    if (splashScreen && splashScreen->isVisible())
    {
        // As splash can go behind other widgets (so it does not obstruct startup debugging)
        // Make it show when a new message is set to it. This should keep it on top for the startup time,
        // but allow you to make it go to the background if you focuse something in front of it.
        splashScreen->activateWindow();

        // Call QApplication::processEvents() to update splash painting as at this point main loop is not running yet
        QString finalMessage = "v" + framework->ApplicationVersion()->GetVersion() + " - " + message.toUpper();
        splashScreen->showMessage(finalMessage, Qt::AlignBottom|Qt::AlignLeft, QColor(240, 240, 240));
        processEvents();
    }
}

QStringList Application::GetQmFiles(const QDir& dir)
{
     QStringList fileNames = dir.entryList(QStringList("*.qm"), QDir::Files, QDir::Name);
     QMutableStringListIterator i(fileNames);
     while (i.hasNext())
     {
         i.next();
         i.setValue(dir.filePath(i.value()));
     }
     return fileNames;
}

void Application::Go()
{
    if (splashScreen)
    {
        splashScreen->close();
        SAFE_DELETE(splashScreen);
    }

    installEventFilter(this);

    QObject::connect(&frameUpdateTimer, SIGNAL(timeout()), this, SLOT(UpdateFrame()));
    frameUpdateTimer.setSingleShot(true);
    frameUpdateTimer.start(0);

    try
    {
        exec();
    }
    catch(const std::exception &e)
    {
        LogError(std::string("Application::Go caught an exception: ") + (e.what() ? e.what() : "(null)"));
        throw;
    }
    catch(...)
    {
        LogError(std::string("Application::Go caught an unknown exception!"));
        throw;
    }
}

void Application::Message(const std::string &title, const std::string &text)
{
#ifdef WIN32
    MessageBoxA(0, text.c_str(), title.c_str(), MB_OK | MB_ICONERROR | MB_TASKMODAL);
#else
    std::cerr << "Application::Message not implemented for current platform!" << std::endl;
    assert(false && "Not implemented!");
#endif
}

void Application::Message(const std::wstring &title, const std::wstring &text)
{
#ifdef WIN32
    MessageBoxW(0, text.c_str(), title.c_str(), MB_OK | MB_ICONERROR | MB_TASKMODAL);
#else
    std::cerr << "Application::Message not implemented for current platform!" << std::endl;
    assert(false && "Not implemented!");
#endif
}

void Application::SetCurrentWorkingDirectory(QString newCwd)
{
    ///\todo UNICODE support!
    boost::filesystem::current_path(newCwd.toStdString());
}

QString Application::CurrentWorkingDirectory()
{
#ifdef _WINDOWS
    WCHAR str[MAX_PATH+1] = {};
    GetCurrentDirectoryW(MAX_PATH, str);
    QString qstr = WStringToQString(str);
    if (!qstr.endsWith('\\'))
        qstr += '\\';
#else
    QString qstr =  QDir::currentPath();
    if (!qstr.endsWith('/'))
        qstr += '/';
#endif

    return qstr;
}

QString Application::InstallationDirectory()
{
    // When running from a debugger, the current directory may in fact be the install directory.
    // Check for the presence of a special tag file to see if we should treat cwd as the installation directory
    // instead of the directory where the current .exe resides.
    QString cwd = CurrentWorkingDirectory();
    if (QFile::exists(cwd + "plugins/TundraInstallationDirectory.txt"))
        return cwd;

#ifdef _WINDOWS
    WCHAR str[MAX_PATH+1] = {};
    DWORD success = GetModuleFileNameW(0, str, MAX_PATH);
    if (success == 0)
        return "";
    QString qstr = WStringToQString(str);
    // The module file name also contains the name of the executable, so strip it off.
    int trailingSlash = qstr.lastIndexOf('\\');
    if (trailingSlash == -1)
        return ""; // Some kind of error occurred.

    return qstr.left(trailingSlash+1); // +1 so that we return the trailing slash as well.
#else
    ///\todo Implement.
    return ".";
#endif
}

QString Application::UserDataDirectory()
{
    const QString applicationName = "Tundra"; ///\todo Move applicationName from Config API to the Application class. Make it static without a runtime setter.
#ifdef _WINDOWS
    LPITEMIDLIST pidl;

    if (SHGetFolderLocation(0, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, 0, &pidl) != S_OK)
        return "";

    WCHAR str[MAX_PATH+1] = {};
    SHGetPathFromIDListW(pidl, str);
    CoTaskMemFree(pidl);

    return WStringToQString(str) + "\\" + applicationName;
#else
    ///\todo Convert to QString instead of std::string.
    char *ppath = 0;
    ppath = getenv("HOME");
    if (ppath == 0)
        throw Exception("Failed to get HOME environment variable.");

    std::string path(ppath);
    return QString((path + "/." + applicationName.toStdString()).c_str());
#endif
}

QString Application::UserDocumentsDirectory()
{
#ifdef _WINDOWS
    LPITEMIDLIST pidl;

    if (SHGetFolderLocation(0, CSIDL_PERSONAL | CSIDL_FLAG_CREATE, NULL, 0, &pidl) != S_OK)
        return "";

    WCHAR str[MAX_PATH+1] = {};
    SHGetPathFromIDListW(pidl, str);
    CoTaskMemFree(pidl);

    const QString applicationName = "Tundra"; ///\todo Move applicationName from Config API to the Application class. Make it static without a runtime setter.
    return WStringToQString(str) + '\\' + applicationName;
#else
    ///\todo Review. Is this desirable?
    return UserDataDirectory();
#endif
}

QString Application::ParseWildCardFilename(const QString& input)
{
    // Parse all the special symbols from the log filename.
    QString filename = input.trimmed().replace("$(CWD)", CurrentWorkingDirectory(), Qt::CaseInsensitive);
    filename = filename.replace("$(INSTDIR)", InstallationDirectory(), Qt::CaseInsensitive);
    filename = filename.replace("$(USERDATA)", UserDataDirectory(), Qt::CaseInsensitive);
    filename = filename.replace("$(USERDOCS)", UserDocumentsDirectory(), Qt::CaseInsensitive);
    QRegExp rx("\\$\\(DATE:(.*)\\)");
    // Qt Regexes don't support non-greedy matching. The above regex should be "\\$\\(DATE:(.*?)\\)". Instad Qt supports
    // only setting the matching to be non-greedy globally.
    rx.setMinimal(true); // This is to avoid e.g. $(DATE:yyyyMMdd)_aaa).txt to be incorrectly captured as "yyyyMMdd)_aaa".
    for(;;) // Loop and find all instances of $(DATE:someformat).
    {
        int pos = rx.indexIn(filename);
        if (pos > -1)
        {
            QString dateFormat = rx.cap(1);
            QString date = QDateTime::currentDateTime().toString(dateFormat);
            filename = filename.replace(rx.pos(0), rx.cap(0).length(), date);
        }
        else
            break;
    }
    return filename;
}

bool Application::eventFilter(QObject *obj, QEvent *event)
{
#ifdef Q_WS_MAC // workaround for Mac, because mouse events are not received as it ought to be
    QMouseEvent *mouse = dynamic_cast<QMouseEvent*>(event);
    if (mouse)
    {
        if (dynamic_cast<UiMainWindow*>(obj))
        {
            switch(event->type())
            {
            case QEvent::MouseButtonPress:
                framework->Ui()->GraphicsView()->mousePressEvent(mouse);
                break;
            case QEvent::MouseButtonRelease:
                framework->Ui()->GraphicsView()->mouseReleaseEvent(mouse);
                break;
            case QEvent::MouseButtonDblClick:
                framework->Ui()->GraphicsView()->mouseDoubleClickEvent(mouse);
                break;
            case QEvent::MouseMove:
                if (mouse->buttons() == Qt::LeftButton)
                    framework->Ui()->GraphicsView()->mouseMoveEvent(mouse);
                break;
            }
        }
    }
#endif
    try
    {
        if (obj == this)
        {
            if (event->type() == QEvent::ApplicationActivate)
                appActivated = true;
            if (event->type() == QEvent::ApplicationDeactivate)
                appActivated = false;
        }

        return QObject::eventFilter(obj, event);
    }
    catch(const std::exception &e)
    {
        std::cout << std::string("QApp::eventFilter caught an exception: ") + (e.what() ? e.what() : "(null)") << std::endl;
        LogError(std::string("QApp::eventFilter caught an exception: ") + (e.what() ? e.what() : "(null)"));
        throw;
    } catch(...)
    {
        std::cout << std::string("QApp::eventFilter caught an unknown exception!") << std::endl;
        LogError(std::string("QApp::eventFilter caught an unknown exception!"));
        throw;
    }
}

void Application::ChangeLanguage(const QString& file)
{
    QString filename = file;
    if (!filename.endsWith(".qm", Qt::CaseInsensitive))
        filename.append(".qm");
    QString tmp = filename;
    tmp.chop(3);
    QString str = tmp.right(2);
    
    QString name = "data/translations/qt_native_translations/qt_" + str + ".qm";

    // Remove old translators then change them to new. 
    removeTranslator(nativeTranslator); 

    if (QFile::exists(name))
    {
        if (nativeTranslator != 0)
        {
            nativeTranslator->load(name);
            installTranslator(nativeTranslator); 
        }
    }
    else
    {
        if (nativeTranslator != 0 && nativeTranslator->isEmpty())
        {
            installTranslator(nativeTranslator);
        }
        else
        {
            SAFE_DELETE(nativeTranslator);
            nativeTranslator = new QTranslator;
            installTranslator(nativeTranslator); 
        }
    }

    // Remove old translators then change them to new. 
    removeTranslator(appTranslator);
    if (appTranslator->load(filename))
    {
        installTranslator(appTranslator);
        framework->Config()->Set(ConfigAPI::FILE_FRAMEWORK, ConfigAPI::SECTION_FRAMEWORK, "language", file);
    }
    
    emit LanguageChanged();
}

bool Application::notify(QObject *receiver, QEvent *event)
{
    try
    {
        return QApplication::notify(receiver, event);
    } catch(const std::exception &e)
    {
        std::cout << std::string("QApp::notify caught an exception: ") << (e.what() ? e.what() : "(null)") << std::endl;
        LogError(std::string("QApp::notify caught an exception: ") + (e.what() ? e.what() : "(null)"));
        throw;
    } catch(...)
    {
        std::cout << std::string("QApp::notify caught an unknown exception!") << std::endl;
        LogError(std::string("QApp::notify caught an unknown exception!"));
        throw;
    }
}

void Application::UpdateFrame()
{
    // Don't pump the QEvents to QApplication if we are exiting
    // also don't process our mainloop frames.
    if (framework->IsExiting())
        return;

    try
    {
        const tick_t frameStartTime = GetCurrentClockTime();

        QApplication::processEvents(QEventLoop::AllEvents, 1);
        QApplication::sendPostedEvents();

        framework->ProcessOneFrame();

        double targetFpsLimit = 60.0;
        QStringList fpsLimitParam = framework->CommandLineParameters("--fpslimit");
        if (fpsLimitParam.size() > 0)
        {
            bool ok;
            double target = fpsLimitParam.first().toDouble(&ok);
            if (ok)
                targetFpsLimit = target;
            if (targetFpsLimit < 1.f)
                targetFpsLimit = 0.f;
        }

        tick_t timeNow = GetCurrentClockTime();

        static tick_t timerFrequency = GetCurrentClockFreq();

        double msecsSpentInFrame = (double)(timeNow - frameStartTime) * 1000.0 / timerFrequency;
        const double msecsPerFrame = 1000.0 / targetFpsLimit;

        ///\note Ideally we should sleep 0 msecs when running at a high fps rate,
        /// but need to avoid QTimer::start() with 0 msecs, since that will cause the timer to immediately fire,
        /// which can cause the Win32 message loop inside Qt to starve. (Qt keeps spinning the timer.start(0) loop for Tundra mainloop and neglects Win32 API).
        double msecsToSleep = std::min(std::max(1.0, msecsPerFrame - msecsSpentInFrame), msecsPerFrame);

        // Reduce frame rate when unfocused
        if (!frameUpdateTimer.isActive())
        {
            if (appActivated || framework->IsHeadless())
                frameUpdateTimer.start((int)msecsToSleep); 
            else 
                frameUpdateTimer.start((int)(msecsToSleep + msecsPerFrame)); // Proceed at half FPS speed when unfocused (but never at half FPS when running a headless server).
        }
    }
    catch(const std::exception &e)
    {
        std::cout << "QApp::UpdateFrame caught an exception: " << (e.what() ? e.what() : "(null)") << std::endl;
        LogError(std::string("QApp::UpdateFrame caught an exception: ") + (e.what() ? e.what() : "(null)"));
        throw;
    }
    catch(...)
    {
        std::cout << "QApp::UpdateFrame caught an unknown exception!" << std::endl;
        LogError(std::string("QApp::UpdateFrame caught an unknown exception!"));
        throw;
    }
}

void Application::AboutToExit()
{
    emit ExitRequested();
    
    // If no-one canceled the exit as a response to the signal, exit
    if (framework->IsExiting())
        quit();
}
