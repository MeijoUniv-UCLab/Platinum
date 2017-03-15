/*****************************************************************
|
|   Platinum - UPnP Engine
|
| Copyright (c) 2004-2010, Plutinosoft, LLC.
| All rights reserved.
| http://www.plutinosoft.com
|
  2014/10/04 ume
 
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
****************************************************************/

/** @file
 UPnP Devices and ControlPoints Manager
 */

#ifndef _PLT_UPNP_H_
#define _PLT_UPNP_H_

/*----------------------------------------------------------------------
|   includes
+---------------------------------------------------------------------*/
#include "PltTaskManager.h"
#include "PltCtrlPoint.h"
#include "PltDeviceHost.h"
#include "PltUtilities.h"

/*----------------------------------------------------------------------
|   constants
+---------------------------------------------------------------------*/
#define PLT_DLNA_SSDP_DELAY       0.05f
#define PLT_DLNA_SSDP_DELAY_GROUP 0.2f

/*----------------------------------------------------------------------
|   forward definitions
+---------------------------------------------------------------------*/
class PLT_SsdpListenTask;

/*----------------------------------------------------------------------
|   PLT_UPnP class
+---------------------------------------------------------------------*/
/**
 The PLT_UPnP class maintains a list of devices (PLT_DeviceHost) to advertise and/or 
 control points (PLT_CtrlPoint).
 */
class PLT_UPnP
{
public:
    /**
     Create a UPnP instance.
     */
    PLT_UPnP();
    ~PLT_UPnP();

    /**
     Add and start a device inside this UPnP context.
     @param device device to start.
     */
    NPT_Result AddDevice(PLT_DeviceHostReference& device);
    
    /**
     Add and start a control point inside this UPnP context.
     @param ctrlpoint control point to start.
     */
    NPT_Result AddCtrlPoint(PLT_CtrlPointReference& ctrlpoint);

    /**
     Remove an existing device from this UPnP context.
     @param device device to stop.
     */
    NPT_Result RemoveDevice(PLT_DeviceHostReference& device);
    
    /**
     Remove an existing control point from this UPnP context.
     @param ctrlpoint control point to stop.
     */
    NPT_Result RemoveCtrlPoint(PLT_CtrlPointReference& ctrlpoint);

    /**
     Start the UPnP context and all existing devices and control points
     associated with it.
     */
    NPT_Result Start();
    
    /**
     Stop the UPnP context and all existing devices and control points
     associated with it.
     */
    NPT_Result Stop();
    
    /**
     Return the UPnP Engine state.
     @return True if the UPnP engine is running.
     */
    bool IsRunning() { return m_Started; }

    /**
     When a device and a control point are added to the same UPnP context, it is
     desired that the device be not discovered by the control point. For example when
     creating a combo UPnP Renderer/CtrlPoint. This methods tells the control point
     to ignore devices associated with the same UPnP context.
     @param ignore boolean to ignore devices in context
     */
	void SetIgnoreLocalUUIDs(bool ignore) { m_IgnoreLocalUUIDs = ignore; }
    
    /**
     * @brief ctrlPointからrootDevice（発見されたデバイスのリスト）を取得
     * @author Rina Umeyama
     * @return rootDevice
     **/
    NPT_List<PLT_DeviceDataReference> getRootDevice();
    
    /**
     * @brief rootDeviceからdeviceData（発見されたデバイスの情報のリスト）の先頭を取得
     * @author Rina Umeyama
     * @return deviceData
     **/
    const PLT_DeviceDataReference getDeviceData();
    
    /**
     * @brief rootDeviceから指定した番号のdeviceData（発見されたデバイスの情報のリスト）を取得
     * @author Rina Umeyama
     * @param number 取得したいデバイスの番号
     * @return deviceData
     **/
    const PLT_DeviceDataReference getDeviceDatas(int number);
    
    /**
     * @brief deviceDataから全てのFriendlyName（デバイス名）を取得してLogを表示
     * @author Rina Umeyama
     **/
    void friendlyNames();
    
    /**
     * @brief deviceDataから指定した番号のデバイスのFriendlyName（デバイス名）を取得
     * @author Rina Umeyama
     * @param number デバイスの番号
     * @return FriendlyNmae
     **/
    const char* getFriendlyNames(int number);
    
    /**
     * @brief rootDeviceに格納されているデバイス数を取得
     * @author Rina Umeyama
     * @return デバイス数
     **/
    int getCount();
    
    /**
     * @brief deviceDataから指定した番号のデバイスのUUIDを取得
     * @author Rina Umeyama
     * @param デバイスの番号
     * @return UUID
     **/
    const char* getUUID(int number);
    
    /**
     * @brief deviceDataから指定した番号のデバイスのデバイスタイプ（DMS，DMR等）を取得
     * @author Rina Umeyama
     * @param number デバイスの番号
     * @return デバイスタイプ
     **/
    const char* getDeviceType(int number);
    
    /**
     * @brief deviceDataから指定した番号のデバイスのActionDesc（アクションDescription）を取得
     * @author Rina Umeyama
     * @param number デバイスの番号
     * @param serviceType（Content Directory等のサービスの種類）
     * @return ActionDesc
     **/
    PLT_ActionDesc* getActionDesc(int number, const char* serviceype);
    
    /**
     * @brief ActionDescから指定した番号のデバイスのArgumentDesc（アクションの引数のDescription）を取得
     * @author Rina Umeyama
     * @param number デバイスの番号
     * @param serviceType サービスの種類（Content Directory等）
     * @param serviceName サービス名（Play, Browse等）
     * @param argumentName Descriptionを取得したい引名
     * @return ArgumentDesc
     **/
    PLT_ArgumentDesc* getArgumentDesc(int number, char* serviceType, char* serviceName, char* argumentName);
    
    /**
     * @brief 指定したArgumentDesc（アクションの引数のDescription）のrelatedStateVariableを取得
     * @author Rina Umeyama
     * @param argumentDesc
     * @return relatedStateVariable
     **/
    const char* getArgument_DeviceType(PLT_ArgumentDesc* argumentDesc);

private:
    // members
    NPT_Mutex                           m_Lock;
    NPT_List<PLT_DeviceHostReference>   m_Devices;
    NPT_List<PLT_CtrlPointReference>    m_CtrlPoints;
    NPT_Reference<PLT_TaskManager>      m_TaskManager;

    // Since we can only have one socket listening on port 1900, 
    // we create it in here and we will attach every control points
    // and devices to it when they're added
    bool                                m_Started;
    PLT_SsdpListenTask*                 m_SsdpListenTask; 
	bool								m_IgnoreLocalUUIDs;
};

#endif /* _PLT_UPNP_H_ */
