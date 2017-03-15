/*****************************************************************
|
|   Platinum - UPnP Engine
|
| Copyright (c) 2004-2010, Plutinosoft, LLC.
| All rights reserved.
| http://www.plutinosoft.com
|
| This program is free software; you can redistribute it and/or
| modify it under the terms of the GNU General Public License
| as published by the Free Software Foundation; either version 2
| of the License, or (at your option) any later version.
|
| OEMs, ISVs, VARs and other distributors that combine and 
| distribute commercially licensed software with Platinum software
| and do not wish to distribute the source code for the commercially
| licensed software under version 2, or (at your option) any later
| version, of the GNU General Public License (the "GPL") must enter
| into a commercial license agreement with Plutinosoft, LLC.
| licensing@plutinosoft.com
|  
| This program is distributed in the hope that it will be useful,
| but WITHOUT ANY WARRANTY; without even the implied warranty of
| MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
| GNU General Public License for more details.
|
| You should have received a copy of the GNU General Public License
| along with this program; see the file LICENSE.txt. If not, write to
| the Free Software Foundation, Inc., 
| 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
| http://www.gnu.org/licenses/gpl-2.0.html
|
|
****************************************************************/

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "Neptune.h"
#include "PltVersion.h"
#include "PltUPnP.h"
#include "PltDeviceHost.h"
#include "PltCtrlPoint.h"
#include "PltSsdp.h"

NPT_SET_LOCAL_LOGGER("platinum.core.upnp")

/*----------------------------------------------------------------------
|   PLT_UPnP_CtrlPointStartIterator class
+---------------------------------------------------------------------*/
class PLT_UPnP_CtrlPointStartIterator
{
public:
    PLT_UPnP_CtrlPointStartIterator(PLT_SsdpListenTask* listen_task) :
        m_ListenTask(listen_task)  {}
    virtual ~PLT_UPnP_CtrlPointStartIterator() {}

    NPT_Result operator()(PLT_CtrlPointReference& ctrl_point) const {
        NPT_CHECK_SEVERE(ctrl_point->Start(m_ListenTask));
        return NPT_SUCCESS;
    }

private:
    PLT_SsdpListenTask* m_ListenTask;
};

/*----------------------------------------------------------------------
|   PLT_UPnP_CtrlPointStopIterator class
+---------------------------------------------------------------------*/
class PLT_UPnP_CtrlPointStopIterator
{
public:
    PLT_UPnP_CtrlPointStopIterator(PLT_SsdpListenTask* listen_task) :
        m_ListenTask(listen_task)  {}
    virtual ~PLT_UPnP_CtrlPointStopIterator() {}

    NPT_Result operator()(PLT_CtrlPointReference& ctrl_point) const {
        return ctrl_point->Stop(m_ListenTask);
    }


private:
    PLT_SsdpListenTask* m_ListenTask;
};

/*----------------------------------------------------------------------
|   PLT_UPnP_DeviceStartIterator class
+---------------------------------------------------------------------*/
class PLT_UPnP_DeviceStartIterator
{
public:
    PLT_UPnP_DeviceStartIterator(PLT_SsdpListenTask* listen_task) :
        m_ListenTask(listen_task)  {}
    virtual ~PLT_UPnP_DeviceStartIterator() {}

    NPT_Result operator()(PLT_DeviceHostReference& device_host) const {
        
        // We should always increment the boot id on restart
        // so it is used in place of boot id during initial announcement
        device_host->SetBootId(device_host->GenerateNextBootId());
        device_host->SetNextBootId(0);
        
        NPT_CHECK_SEVERE(device_host->Start(m_ListenTask));
        return NPT_SUCCESS;
    }

private:
    PLT_SsdpListenTask* m_ListenTask;
};

/*----------------------------------------------------------------------
|   PLT_UPnP_DeviceStopIterator class
+---------------------------------------------------------------------*/
class PLT_UPnP_DeviceStopIterator
{
public:
    PLT_UPnP_DeviceStopIterator(PLT_SsdpListenTask* listen_task) :
        m_ListenTask(listen_task)  {}
    virtual ~PLT_UPnP_DeviceStopIterator() {}

    NPT_Result operator()(PLT_DeviceHostReference& device_host) const {
        return device_host->Stop(m_ListenTask);
    }


private:
    PLT_SsdpListenTask* m_ListenTask;
};

/*----------------------------------------------------------------------
|   PLT_UPnP::PLT_UPnP
+---------------------------------------------------------------------*/
PLT_UPnP::PLT_UPnP() :
    m_TaskManager(NULL),
    m_Started(false),
    m_SsdpListenTask(NULL),
	m_IgnoreLocalUUIDs(true)
{
}
    
/*----------------------------------------------------------------------
|   PLT_UPnP::~PLT_UPnP
+---------------------------------------------------------------------*/
PLT_UPnP::~PLT_UPnP()
{
    Stop();

    m_CtrlPoints.Clear();
    m_Devices.Clear();
}

/*----------------------------------------------------------------------
|   PLT_UPnP::Start()
+---------------------------------------------------------------------*/
NPT_Result
PLT_UPnP::Start()
{
    NPT_LOG_INFO("Starting UPnP...");

    NPT_AutoLock lock(m_Lock);

    if (m_Started) NPT_CHECK_WARNING(NPT_ERROR_INVALID_STATE);
    
    NPT_List<NPT_IpAddress> ips;
    PLT_UPnPMessageHelper::GetIPAddresses(ips);
    
    /* Create multicast socket and bind on 1900. If other apps didn't
       play nicely by setting the REUSE_ADDR flag, this could fail */
    NPT_Reference<NPT_UdpMulticastSocket> socket(new NPT_UdpMulticastSocket());
    NPT_CHECK_SEVERE(socket->Bind(NPT_SocketAddress(NPT_IpAddress::Any, 1900), true));
    
    
    /* Join multicast group for every ip we found */
    NPT_CHECK_SEVERE(ips.ApplyUntil(PLT_SsdpInitMulticastIterator(socket.AsPointer()),
                                    NPT_UntilResultNotEquals(NPT_SUCCESS)));
    

    /* create the ssdp listener */
    m_SsdpListenTask = new PLT_SsdpListenTask(socket.AsPointer());
    socket.Detach();
    NPT_Reference<PLT_TaskManager> taskManager(new PLT_TaskManager());
    NPT_CHECK_SEVERE(taskManager->StartTask(m_SsdpListenTask));
    
    /* start devices & ctrlpoints */
    m_CtrlPoints.Apply(PLT_UPnP_CtrlPointStartIterator(m_SsdpListenTask));
    m_Devices.Apply(PLT_UPnP_DeviceStartIterator(m_SsdpListenTask));
    
    NPT_CHECK_SEVERE(NPT_SUCCESS);

    m_TaskManager = taskManager;
    m_Started = true;
    return NPT_SUCCESS;
}

/*----------------------------------------------------------------------
|   PLT_UPnP::Stop
+---------------------------------------------------------------------*/
NPT_Result
PLT_UPnP::Stop()
{
    NPT_AutoLock lock(m_Lock);

    if (!m_Started) NPT_CHECK_WARNING(NPT_ERROR_INVALID_STATE);

    NPT_LOG_INFO("Stopping UPnP...");

    // Stop ctrlpoints and devices first
    m_CtrlPoints.Apply(PLT_UPnP_CtrlPointStopIterator(m_SsdpListenTask));
    m_Devices.Apply(PLT_UPnP_DeviceStopIterator(m_SsdpListenTask));

    // stop remaining tasks
    m_TaskManager->Abort();
    m_SsdpListenTask = NULL;
    m_TaskManager = NULL;

    m_Started = false;
    return NPT_SUCCESS;
}

/*----------------------------------------------------------------------
|   PLT_UPnP::AddDevice
+---------------------------------------------------------------------*/
NPT_Result
PLT_UPnP::AddDevice(PLT_DeviceHostReference& device)
{
    NPT_AutoLock lock(m_Lock);

    // tell all our controllers to ignore this device
	if (m_IgnoreLocalUUIDs) {
		for (NPT_List<PLT_CtrlPointReference>::Iterator iter = 
                 m_CtrlPoints.GetFirstItem(); 
             iter; 
             iter++) {
		    (*iter)->IgnoreUUID(device->GetUUID());
		}
	}

    if (m_Started) {
        NPT_LOG_INFO("Starting Device...");
        NPT_CHECK_SEVERE(device->Start(m_SsdpListenTask));
    }

    m_Devices.Add(device);
    return NPT_SUCCESS;
}

/*----------------------------------------------------------------------
|   PLT_UPnP::RemoveDevice
+---------------------------------------------------------------------*/
NPT_Result
PLT_UPnP::RemoveDevice(PLT_DeviceHostReference& device)
{
    NPT_AutoLock lock(m_Lock);

    if (m_Started) {
        device->Stop(m_SsdpListenTask);
    }

    return m_Devices.Remove(device);
}

/*----------------------------------------------------------------------
|   PLT_UPnP::AddCtrlPoint
+---------------------------------------------------------------------*/
NPT_Result
PLT_UPnP::AddCtrlPoint(PLT_CtrlPointReference& ctrl_point)
{
    NPT_AutoLock lock(m_Lock);

    // tell the control point to ignore our own running devices
	if (m_IgnoreLocalUUIDs) {
		for (NPT_List<PLT_DeviceHostReference>::Iterator iter = 
                 m_Devices.GetFirstItem(); 
             iter; 
             iter++) {
			ctrl_point->IgnoreUUID((*iter)->GetUUID());
		}
	}

    if (m_Started) {
        NPT_LOG_INFO("Starting Ctrlpoint...");
        NPT_CHECK_SEVERE(ctrl_point->Start(m_SsdpListenTask));
    }

    m_CtrlPoints.Add(ctrl_point);
    return NPT_SUCCESS;
}

/*----------------------------------------------------------------------
|   PLT_UPnP::RemoveCtrlPoint
+---------------------------------------------------------------------*/
NPT_Result
PLT_UPnP::RemoveCtrlPoint(PLT_CtrlPointReference& ctrl_point)
{
    NPT_AutoLock lock(m_Lock);

    if (m_Started) {
        ctrl_point->Stop(m_SsdpListenTask);
    }

    return m_CtrlPoints.Remove(ctrl_point);
}

// add by ume
NPT_List<PLT_DeviceDataReference>
PLT_UPnP::getRootDevice()
{
    NPT_List<NPT_Reference<PLT_CtrlPoint> >::Iterator ctrlPoint = m_CtrlPoints.GetFirstItem();
    NPT_Reference<PLT_CtrlPoint>* data = ctrlPoint.operator->();
    PLT_CtrlPoint* object = data->operator->();
    NPT_List<PLT_DeviceDataReference> root_device = object->m_RootDevices;
    
    return root_device;
}

const PLT_DeviceDataReference
PLT_UPnP::getDeviceData()
{
    NPT_List<PLT_DeviceDataReference> root_device = getRootDevice();
    NPT_List<PLT_DeviceDataReference>::Iterator deviceData = root_device.GetFirstItem();
    return deviceData.operator*();
}

const PLT_DeviceDataReference
PLT_UPnP::getDeviceDatas(int number)
{
    NPT_List<PLT_DeviceDataReference> root_device = getRootDevice();
    NPT_List<PLT_DeviceDataReference>::Iterator deviceData = root_device.GetFirstItem();
    int i=0;
    while (i<number) {
        deviceData.operator++();
        i++;
    }
   return deviceData.operator*();
}

// FriendlyName Log
void
PLT_UPnP::friendlyNames()
{
    NPT_List<PLT_DeviceDataReference> root_device = getRootDevice();
    
    int count = root_device.GetItemCount();
    NPT_LOG_INFO_1("deviceCount:%d", count);
    
    NPT_List<PLT_DeviceDataReference>::Iterator deviceData = root_device.GetFirstItem();
    for (int i=1; i<=count; i++) {
        PLT_DeviceDataReference& data_device = deviceData.operator*();
        NPT_LOG_INFO_1("%s", (const char*)data_device->GetFriendlyName());
        deviceData.operator++();
    }
}

// return FriendlyName
const char*
PLT_UPnP::getFriendlyNames(int number)
{
    NPT_List<PLT_DeviceDataReference> root_device = getRootDevice();
    NPT_List<PLT_DeviceDataReference>::Iterator deviceData = root_device.GetFirstItem();
    int i=0;
    while (i<number) {
        deviceData.operator++();
        i++;
    }
    PLT_DeviceDataReference& data_device = deviceData.operator*();
    return (const char*)data_device->GetFriendlyName();
}

int
PLT_UPnP::getCount()
{
    NPT_List<PLT_DeviceDataReference> root_device = getRootDevice();
    return root_device.GetItemCount();
}

const char*
PLT_UPnP::getUUID(int number)
{
    NPT_List<PLT_DeviceDataReference> root_device = getRootDevice();
    NPT_List<PLT_DeviceDataReference>::Iterator deviceData = root_device.GetFirstItem();
    int i=0;
    while (i<number) {
        deviceData.operator++();
        i++;
    }
    PLT_DeviceDataReference& data_device = deviceData.operator*();
    return (const char*)data_device->GetUUID();
}

const char*
PLT_UPnP::getDeviceType(int number)
{
    NPT_List<PLT_DeviceDataReference> root_device = getRootDevice();
    NPT_List<PLT_DeviceDataReference>::Iterator deviceData = root_device.GetFirstItem();
    int i=0;
    while (i<number) {
        deviceData.operator++();
        i++;
    }
    PLT_DeviceDataReference& data_device = deviceData.operator*();
    return (const char*)data_device->GetType();
}

PLT_ActionDesc*
PLT_UPnP::getActionDesc(int number, const char* serviceType)
{
    NPT_List<PLT_DeviceDataReference> root_device = getRootDevice();
    NPT_List<PLT_DeviceDataReference>::Iterator deviceData = root_device.GetFirstItem();
    int i=0;
    while (i<number) {
        deviceData.operator++();
        i++;
    }
    PLT_DeviceDataReference& data_device = deviceData.operator*();
    NPT_Array<PLT_Service*> service_array = data_device->GetServices();
    int count = service_array.GetItemCount();
    PLT_Service* service;
    PLT_ActionDesc *actionDesc_item;
    
    i=0;
    while (i<count) {
        service = service_array.operator[](i);
        const char* service_type;
        service_type = service->GetServiceType();
        if (strstr(service_type, serviceType) != NULL) {
            NPT_Array<PLT_ActionDesc*> actionDesc = service->GetActionDescs();
            int count_a = actionDesc.GetItemCount();
            int j=0;
            while (j<count_a) {
                actionDesc_item = actionDesc.operator[](j);
                return actionDesc_item;
            }
        }
        i++;
    }
    return actionDesc_item;
}

/*
 
*/
PLT_ArgumentDesc*
PLT_UPnP::getArgumentDesc(int number, char* serviceType, char* serviceName, char* argumentName)
{
    NPT_List<PLT_DeviceDataReference> root_device = getRootDevice();
    NPT_List<PLT_DeviceDataReference>::Iterator deviceData = root_device.GetFirstItem();
    int i=0;
    while (i<number) {
        deviceData.operator++();
        i++;
    }
    PLT_DeviceDataReference& data_device = deviceData.operator*();
    NPT_Array<PLT_Service*> service_array = data_device->GetServices();
    int count = service_array.GetItemCount();
    PLT_Service* service;
    PLT_ActionDesc *actionDesc_item;
    PLT_ArgumentDesc *argumentDesc;
    
    i=0;
    while (i<count) {
        service = service_array.operator[](i);
        const char* service_type;
        service_type = service->GetServiceType();
        if (strstr(service_type, serviceType) != NULL) {
            NPT_Array<PLT_ActionDesc*> actionDesc = service->GetActionDescs();
            int count_a = actionDesc.GetItemCount();
            int j=0;
            while (j<count_a) {
                actionDesc_item = actionDesc.operator[](j);
                if (strstr(actionDesc_item->GetName(), serviceName)) {
                    argumentDesc = actionDesc_item->GetArgumentDesc(argumentName);
                    return argumentDesc;
                    break;
                }
            }
        }
        i++;
    }
    return argumentDesc;
}

const char*
PLT_UPnP::getArgument_DeviceType(PLT_ArgumentDesc* argumentDesc)
{
    PLT_StateVariable* stateVar = argumentDesc->GetRelatedStateVariable();
    return stateVar->GetDataType();
}
