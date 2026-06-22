/**
 * DiscoveryApi.h - actively discover foobar2000 component services
 * 
 * Exposes discovery of services provided by other foobar2000 components:
 * - main menu commands
 * - context-menu items
 * - input formats / decoders
 * - DSP processors
 * - visualization components
 * - UI elements
 */

/**
 * DiscoveryApi.h - Proactive foobar2000 Service Discovery API
 * 
 * Provides the ability to enumerate various services in foobar2000,
 * allowing the frontend to discover functionality from other components.
 * 
 * APIs:
 * - discovery.getAllServices              Get summary of all discoverable services
 * - discovery.getMainMenuCommands         Get all main menu commands
 * - discovery.getMainMenuGroups           Get main menu groups
 * - discovery.executeMainMenuCommand      Execute a main menu command by GUID
 * - discovery.getContextMenuCommands      Get all context menu commands (external plugins)
 * - discovery.executeContextMenuCommand   Execute context menu on current track
 * - discovery.getInputFormats             Get supported input formats (audio codecs)
 * - discovery.getComponents               Get all installed components
 * - discovery.getUIElements               Get UI elements
 * - discovery.getDspEntries               Get DSP processors
 * - discovery.getOutputDevices            Get output devices
 * - discovery.getPreferencePages          Get preference pages
 * - discovery.searchCommands              Search menu commands by name/description
 * - discovery.executeContextMenuByPath    Execute context menu command by path
 * - discovery.getContextMenuTree          Get context menu tree structure
 */

#pragma once

namespace discovery_api {
    /**
     * Register all service discovery APIs
     */
    void RegisterApis();
}
