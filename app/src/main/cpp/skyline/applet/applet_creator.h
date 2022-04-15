// SPDX-License-Identifier: MPL-2.0
// Copyright © 2022 Skyline Team and Contributors (https://github.com/skyline-emu/)

#pragma once

#include <common/macros.h>
#include <services/am/applet/IApplet.h>
#include <services/applet/common_arguments.h>

namespace skyline::applet {
    #define APPLETS                                   \
    APPLET_ENTRY(None,                         0x000) \
    APPLET_ENTRY(Application,                  0x001) \
    APPLET_ENTRY(OverlayApplet,                0x002) \
    APPLET_ENTRY(SystemAppletMenu,             0x003) \
    APPLET_ENTRY(SystemApplication,            0x004) \
    APPLET_ENTRY(LibraryAppletAuth,            0x00A) \
    APPLET_ENTRY(LibraryAppletCabinet,         0x00B) \
    APPLET_ENTRY(LibraryAppletController,      0x00C) \
    APPLET_ENTRY(LibraryAppletDataErase,       0x00D) \
    APPLET_ENTRY(LibraryAppletError,           0x00E) \
    APPLET_ENTRY(LibraryAppletNetConnect,      0x00F) \
    APPLET_ENTRY(LibraryAppletPlayerSelect,    0x010) \
    APPLET_ENTRY(LibraryAppletSwkbd,           0x011) \
    APPLET_ENTRY(LibraryAppletMiiEdit,         0x012) \
    APPLET_ENTRY(LibraryAppletWeb,             0x013) \
    APPLET_ENTRY(LibraryAppletShop,            0x014) \
    APPLET_ENTRY(LibraryAppletPhotoViewer,     0x015) \
    APPLET_ENTRY(LibraryAppletSet,             0x016) \
    APPLET_ENTRY(LibraryAppletOfflineWeb,      0x017) \
    APPLET_ENTRY(LibraryAppletLoginShare,      0x018) \
    APPLET_ENTRY(LibraryAppletWifiWebAuth,     0x019) \
    APPLET_ENTRY(LibraryAppletMyPage,          0x01A) \
    APPLET_ENTRY(LibraryAppletGift,            0x01B) \
    APPLET_ENTRY(LibraryAppletUserMigration,   0x01C) \
    APPLET_ENTRY(LibraryAppletPreomiaSys,      0x01D) \
    APPLET_ENTRY(LibraryAppletStory,           0x01E) \
    APPLET_ENTRY(LibraryAppletPreomiaUsr,      0x01F) \
    APPLET_ENTRY(LibraryAppletPreomiaUsrDummy, 0x020) \
    APPLET_ENTRY(LibraryAppletSample,          0x021) \
    APPLET_ENTRY(DevlopmentTool,               0x3E8) \
    APPLET_ENTRY(CombinationLA,                0x3F1) \
    APPLET_ENTRY(AeSystemApplet,               0x3F2) \
    APPLET_ENTRY(AeOverlayApplet,              0x3F3) \
    APPLET_ENTRY(AeStarter,                    0x3F4) \
    APPLET_ENTRY(AeLibraryAppletAlone,         0x3F5) \
    APPLET_ENTRY(AeLibraryApplet1,             0x3F6) \
    APPLET_ENTRY(AeLibraryApplet2,             0x3F7) \
    APPLET_ENTRY(AeLibraryApplet3,             0x3F8) \
    APPLET_ENTRY(AeLibraryApplet4,             0x3F9) \
    APPLET_ENTRY(AppletISA,                    0x3FA) \
    APPLET_ENTRY(AppletIOA,                    0x3FB) \
    APPLET_ENTRY(AppletISTA,                   0x3FC) \
    APPLET_ENTRY(AppletILA1,                   0x3FD) \
    APPLET_ENTRY(AppletILA2,                   0x3FE)

    /**
      * @url https://switchbrew.org/wiki/Applet_Manager_services#AppletId
      */
    enum class AppletId : u32 {
        #define APPLET_ENTRY(name, id) name = id,
        APPLETS
        #undef APPLET_ENTRY
    };

    #define APPLET_ENTRY(name, id) ENUM_CASE(name);

    ENUM_STRING(AppletId, APPLETS)

    #undef APPLET_ENTRY

    /**
     * @brief Creates an Applet of the appropiate class depending on the AppletId
     */
    std::shared_ptr<service::am::IApplet> CreateApplet(
        const DeviceState &state, service::ServiceManager &manager,
        applet::AppletId appletId, std::shared_ptr<kernel::type::KEvent> onAppletStateChanged,
        std::shared_ptr<kernel::type::KEvent> onNormalDataPushFromApplet,
        std::shared_ptr<kernel::type::KEvent> onInteractiveDataPushFromApplet,
        service::applet::LibraryAppletMode appletMode);
}
