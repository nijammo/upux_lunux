/*
 * tested on ubuntu 22.04.1 LTS
 */


#include "../../rx/rx.h"
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include "handling.h"
#include <iostream>
#include <sstream>
#include "math.h"
#include <chrono>
#include <thread>

// keys: 107 = mouse1, 108 = mouse2, 109 = mouse3, 110 = mouse4, 111 = mouse5, 80 = LAlt 
#define AIMKEY 107

#define AIMFOV 5.0f
#define AIMSMOOTH 18.0f
#define GLOW_ESP 1

std::chrono::milliseconds sleep(10); //aim assist sleep time in miliseconds
float maxdistance = 50.0f; //aim assist maximum range in meters




#define Item_Enable_key 103 //103 = caplock

//lobby: 1987212659

float r = 242.0f;
float g = 31.0f;
float b = 31.0f;

float item_r = 255.0f;
float item_g = 255.0f;
float item_b = 255.0f;


int GetApexProcessId(void)
{
	int pid = 0;
	rx_handle snapshot = rx_create_snapshot(RX_SNAP_TYPE_PROCESS, 0);

	RX_PROCESS_ENTRY entry;

	while (rx_next_process(snapshot, &entry))
	{
		if (!strcmp(entry.name, "wine64-preloader"))
		{
			rx_handle snapshot_2 = rx_create_snapshot(RX_SNAP_TYPE_LIBRARY, entry.pid);

			RX_LIBRARY_ENTRY library_entry;

			while (rx_next_library(snapshot_2, &library_entry))
			{
				if (!strcmp(library_entry.name, "easyanticheat_x64.dll"))
				{
					pid = entry.pid;
					break;
				}
			}
			rx_close_handle(snapshot_2);

			//
			// process found
			//
			if (pid != 0)
			{
				break;
			}
		}
	}
	rx_close_handle(snapshot);

	return pid;
}

QWORD GetApexBaseAddress(int pid)
{
	rx_handle snapshot = rx_create_snapshot(RX_SNAP_TYPE_LIBRARY, pid);

	RX_LIBRARY_ENTRY entry;
	DWORD counter = 0;
	QWORD base = 0;

	while (rx_next_library(snapshot, &entry))
	{
		const char *sub = strstr(entry.name, "memfd:wine-mapping");

		if ((entry.end - entry.start) == 0x1000 && sub)
		{
			if (counter == 0)
				base = entry.start;
		}

		if (sub)
		{
			counter++;
		}

		else
		{
			counter = 0;
			base = 0;
		}

		if (counter >= 200)
		{
			break;
		}
	}

	rx_close_handle(snapshot);

	return base;
}

typedef struct
{
	uint8_t pad1[0xCC];
	float x;
	uint8_t pad2[0xC];
	float y;
	uint8_t pad3[0xC];
	float z;
} matrix3x4_t;

int m_iHealth;
int m_iTeamNum;
int m_iViewAngles;
int m_iCameraAngles;
int m_bZooming;
int m_iBoneMatrix;
int m_iWeapon;
int m_vecAbsOrigin;
int m_playerData;
int m_lifeState;
int itemID;
int mode;
int iTeamControl;
int iLocControl;
int spectatorcount = 0;
#define in_Attack 0x0763f9f0
#define m_bleedoutState 0x2718
#define OFFSET_YAW 0x223C

QWORD GetClientEntity(rx_handle game_process, QWORD entity, QWORD index)
{

	index = index + 1;
	index = index << 0x5;

	return rx_read_i64(game_process, (index + entity) - 0x280050);
}

QWORD get_interface_function(rx_handle game_process, QWORD ptr, DWORD index)
{
	return rx_read_i64(game_process, rx_read_i64(game_process, ptr) + index * 8);
}

vec3 GetBonePosition(rx_handle game_process, QWORD entity_address, int index)
{
	vec3 position;
	rx_read_process(game_process, entity_address + m_vecAbsOrigin, &position, sizeof(position));

	QWORD bonematrix = rx_read_i64(game_process, entity_address + m_iBoneMatrix);

	matrix3x4_t matrix;
	rx_read_process(game_process, bonematrix + (0x30 * index), &matrix, sizeof(matrix3x4_t));

	vec3 bonepos;
	bonepos.x = matrix.x + position.x;
	bonepos.y = matrix.y + position.y;
	bonepos.z = matrix.z + position.z;

	return bonepos;
}

BOOL IsButtonDown(rx_handle game_process, QWORD IInputSystem, int KeyCode)
{
	KeyCode = KeyCode + 1;
	DWORD a0 = rx_read_i32(game_process, IInputSystem + ((KeyCode >> 5) * 4) + 0xb0);
	return (a0 >> (KeyCode & 31)) & 1;
}

int dump_table(rx_handle game_process, QWORD table, const char *name)
{

	for (DWORD i = 0; i < rx_read_i32(game_process, table + 0x10); i++)
	{

		QWORD recv_prop = rx_read_i64(game_process, table + 0x8);
		if (!recv_prop)
		{
			continue;
		}

		recv_prop = rx_read_i64(game_process, recv_prop + 0x8 * i);
		char recv_prop_name[260];
		{
			QWORD name_ptr = rx_read_i64(game_process, recv_prop + 0x28);
			rx_read_process(game_process, name_ptr, recv_prop_name, 260);
		}

		if (!strcmp(recv_prop_name, name))
		{
			return rx_read_i32(game_process, recv_prop + 0x4);
		}
	}

	return 0;
}

int main(void)
{
	int pid = GetApexProcessId();

	if (pid == 0)
	{
		printf("[-] r5apex.exe was not found\n");
		return 0;
	}

	rx_handle r5apex = rx_open_process(pid, RX_ALL_ACCESS);
	if (r5apex == 0)
	{
		printf("[-] unable to attach r5apex.exe\n");
		return 0;
	}

	printf("[+] r5apex.exe pid [%d]\n", pid);

	//
	// get base address
	// in case this function doesn't work, use QWORD base_module = 0x140000000;
	//
	
	QWORD base_module = 0x140000000;
	if (base_module == 0)
	{
		return 0;
		printf("[+] r5apex.exe base [0x%lx]\n", base_module);
	}

	

	printf("[+] r5apex.exe base [0x%lx]\n", base_module);
	

	DWORD dwBulletSpeed = 0, dwBulletGravity = 0, dwMuzzle = 0, dwVisibleTime = 0;

	QWORD base_module_dump = rx_dump_module(r5apex, base_module);
		
 		
	if (base_module_dump == 0)
	{
		printf("[-] failed to dump r5apex.exe\n");
		rx_close_handle(r5apex);
		return 0;
	}

	QWORD IClientEntityList = 0;
	{
		char pattern[] = "\x4C\x8B\x15\x00\x00\x00\x00\x33\xF6";
		char mask[] = "xxx????xx";

		// IClientEntityList = 0x1a203b8 + base_module + 0x280050;
		IClientEntityList = rx_scan_pattern(base_module_dump, pattern, mask, 9);
		if (IClientEntityList)
		{
			IClientEntityList = ResolveRelativeAddressEx(r5apex, IClientEntityList, 3, 7);
			IClientEntityList = IClientEntityList + 0x08;
		}
	}

	QWORD dwLocalPlayer = 0;
	{

		// 89 41 28 48 8B 05 ? ? ? ?
		char pattern[] = "\x89\x41\x28\x48\x8B\x05\x00\x00\x00\x00\x48\x85\xC0";
		char mask[] = "xxxxxx????xxx";
		dwLocalPlayer = rx_scan_pattern(base_module_dump, pattern, mask, 13);
		if (dwLocalPlayer)
		{
			dwLocalPlayer = dwLocalPlayer + 0x03;
			dwLocalPlayer = ResolveRelativeAddressEx(r5apex, dwLocalPlayer, 3, 7);
		}
	}
	
	QWORD IInputSystem = 0;
	{
		// 48 8B 05 ? ? ? ? 48 8D 4C  24 20 BA 01 00 00 00 C7
		char pattern[] = "\x48\x8B\x05\x00\x00\x00\x00\x48\x8D\x4C\x24\x20\xBA\x01\x00\x00\x00\xC7";
		char mask[] = "xxx????xxxxxxxxxxx";

		IInputSystem = rx_scan_pattern(base_module_dump, pattern, mask, 18);
		IInputSystem = ResolveRelativeAddressEx(r5apex, IInputSystem, 3, 7);
		IInputSystem = IInputSystem - 0x10;
	}

	QWORD GetAllClasses = 0;
	{
		// 48 8B 05 ? ? ? ? C3 CC CC CC CC CC CC CC CC 48 89 74 24 20
		char pattern[] = "\x48\x8B\x05\x00\x00\x00\x00\xC3\xCC\xCC\xCC\xCC\xCC\xCC\xCC\xCC\x48\x89\x74\x24\x20";
		char mask[] = "xxx????xxxxxxxxxxxxxx";
		GetAllClasses = rx_scan_pattern(base_module_dump, pattern, mask, 21);
		GetAllClasses = ResolveRelativeAddressEx(r5apex, GetAllClasses, 3, 7);
		GetAllClasses = rx_read_i64(r5apex, GetAllClasses);
	}

	QWORD sensitivity = 0;
		sensitivity = base_module + 0x01eaafd0;

	

	{

		char pattern[] = "\x75\x0F\xF3\x44\x0F\x10\xBF";
		char mask[] = "xxxxxxx";
		QWORD temp_address = rx_scan_pattern(base_module_dump, pattern, mask, 7);
		if (temp_address)
		{

			QWORD bullet_gravity = temp_address + 0x02;
			bullet_gravity = bullet_gravity + 0x05;

			QWORD bullet_speed = temp_address - 0x6D;
			bullet_speed = bullet_speed + 0x04;

			dwBulletSpeed = rx_read_i32(r5apex, bullet_speed);
			dwBulletGravity = rx_read_i32(r5apex, bullet_gravity);
		}
	}

	{
		char pattern[] = "\xF3\x0F\x10\x91\x00\x00\x00\x00\x48\x8D\x04\x40";
		char mask[] = "xxxx????xxxx";

		QWORD temp_address = rx_scan_pattern(base_module_dump, pattern, mask, 12);
		if (temp_address)
		{
			temp_address = temp_address + 0x04;
			dwMuzzle = rx_read_i32(r5apex, temp_address);
		}
	}

	{
		// 48 8B CE  ? ? ? ? ? 84 C0 0F 84 BA 00 00 00
		char pattern[] = "\x48\x8B\xCE\x00\x00\x00\x00\x00\x84\xC0\x0F\x84\xBA\x00\x00\x00";
		char mask[] = "xxx?????xxxxxxxx";
		QWORD vis_time = rx_scan_pattern(base_module_dump, pattern, mask, 16);
		if (vis_time)
		{
			vis_time = vis_time + 0x10;
			dwVisibleTime = rx_read_i32(r5apex, vis_time + 0x4);
			//dwVisibleTime = 0x1a48;
		}
	}

	rx_free_module(base_module_dump);

	while (GetAllClasses)
	{

		QWORD recv_table = rx_read_i64(r5apex, GetAllClasses + 0x18);
		QWORD recv_name = rx_read_i64(r5apex, recv_table + 0x4C8);


		char name[260];
		rx_read_process(r5apex, recv_name, name, 260);


		if (!strcmp(name, "DT_Player"))
		{
			m_iHealth = dump_table(r5apex, recv_table, "m_iHealth");
			m_iViewAngles = dump_table(r5apex, recv_table, "m_ammoPoolCapacity") - 0x14;
			m_bZooming = dump_table(r5apex, recv_table, "m_bZooming");
			m_lifeState = dump_table(r5apex, recv_table, "m_lifeState");
			m_iCameraAngles = dump_table(r5apex, recv_table, "m_zoomFullStartTime") + 0x2EC;
		}

		if (!strcmp(name, "DT_BaseEntity"))
		{
			m_iTeamNum = dump_table(r5apex, recv_table, "m_iTeamNum");
			m_vecAbsOrigin = 0x014c;
		}

		if (!strcmp(name, "DT_BaseCombatCharacter"))
		{
			m_iWeapon = dump_table(r5apex, recv_table, "m_latestPrimaryWeapons");
		}

		if (!strcmp(name, "DT_BaseAnimating"))
		{
			m_iBoneMatrix = dump_table(r5apex, recv_table, "m_nForceBone") + 0x50 - 0x8;
		}

		if (!strcmp(name, "DT_WeaponX"))
		{
			m_playerData = dump_table(r5apex, recv_table, "m_playerData");
		}		

		GetAllClasses = rx_read_i64(r5apex, GetAllClasses + 0x20);
	}
	


	DWORD previous_tick = 0;
	float lastvis_aim[70];
	memset(lastvis_aim, 0, sizeof(lastvis_aim));

	if (IClientEntityList == 0)
	{
		printf("[-] IClientEntityList not found\n");
		goto ON_EXIT;
	}

	if (dwLocalPlayer == 0)
	{
		printf("[-] dwLocalPlayer not found\n");
		goto ON_EXIT;
	}

	if (IInputSystem == 0)
	{
		printf("[-] IInputSystem not found\n");
		goto ON_EXIT;
	}

	if (sensitivity == 0)
	{
		printf("[-] sensitivity not found\n");
		goto ON_EXIT;
	}

	if (dwBulletSpeed == 0)
	{
		printf("[-] dwBulletSpeed not found\n");
		goto ON_EXIT;
	}

	if (dwBulletGravity == 0)
	{
		printf("[-] dwBulletGravity not found\n");
		goto ON_EXIT;
	}

	if (dwMuzzle == 0)
	{
		printf("[-] dwMuzzle not found\n");
		goto ON_EXIT;
	}

	if (dwVisibleTime == 0)
	{
		printf("[-] dwVisibleTime not found\n");
		goto ON_EXIT;
	}

	
	dwMuzzle = dwMuzzle - 0x04;
	
	printf("[+] IClientEntityList: %lx\n", IClientEntityList - base_module);
	printf("[+] dwLocalPlayer: %lx\n", dwLocalPlayer - base_module);
	printf("[+] IInputSystem: %lx\n", IInputSystem - base_module);
	printf("[+] sensitivity: %lx\n", sensitivity - base_module);
	printf("[+] dwBulletSpeed: %x\n", dwBulletSpeed);
	printf("[+] dwBulletGravity: %x\n", dwBulletGravity);
	printf("[+] dwMuzzle: %x\n", dwMuzzle);
	printf("[+] dwVisibleTime: %x\n", dwVisibleTime);
	printf("[+] m_iHealth: %x\n", m_iHealth);
	printf("[+] m_iViewAngles: %x\n", m_iViewAngles);
	printf("[+] m_bZooming: %x\n", m_bZooming);
	printf("[+] m_iCameraAngles: %x\n", m_iCameraAngles);
	printf("[+] m_lifeState: %x\n", m_lifeState);
	printf("[+] m_iTeamNum: %x\n", m_iTeamNum);
	printf("[+] m_vecAbsOrigin: %x\n", m_vecAbsOrigin);
	printf("[+] m_iWeapon: %x\n", m_iWeapon);
	printf("[+] m_iBoneMatrix: %x\n", m_iBoneMatrix);
	printf("[+] m_playerData: %x\n", m_playerData);	
		
 	
	fflush(stdout);
	
	//printf("[+] GameMode: %d", mode);

	while (1)
	{
		if (!rx_process_exists(r5apex))
		{
			break;
		}
		
	uint64_t gameModePtr = rx_read_i32(r5apex, base_module + 0x01e87f30 + 0x58);
 	int gameMode = rx_read_int(r5apex, gameModePtr);
 	//printf("\r[+] Game Mode Int: %d", gameMode);
	//fflush(stdout);
	

		QWORD localplayer = rx_read_i64(r5apex, dwLocalPlayer);

		if (localplayer == 0)
		{
			previous_tick = 0;
			memset(lastvis_aim, 0, sizeof(lastvis_aim));
			continue;
		}

		DWORD local_team = rx_read_i32(r5apex, localplayer + m_iTeamNum);

		float fl_sensitivity = rx_read_float(r5apex, sensitivity + 0x68);
		DWORD weapon_id = rx_read_i32(r5apex, localplayer + m_iWeapon) & 0xFFFF;
		QWORD weapon = GetClientEntity(r5apex, IClientEntityList, weapon_id - 1);

		float bulletSpeed = rx_read_float(r5apex, weapon + dwBulletSpeed);
		float bulletGravity = rx_read_float(r5apex, weapon + dwBulletGravity);

		vec3 muzzle;
		rx_read_process(r5apex, localplayer + dwMuzzle, &muzzle, sizeof(vec3));

		float target_fov = 360.0f;
		QWORD target_entity = 0;

		vec3 local_position;
		rx_read_process(r5apex, localplayer + m_vecAbsOrigin, &local_position, sizeof(vec3));
		
		/*
		for (int k = 0; k < 10000; k++)
		{	
			QWORD entity = GetClientEntity(r5apex, IClientEntityList, k);
			itemID = rx_read_int(r5apex, entity + 0x1628);
			//printf("[+] m_customScriptInt: %x\n", itemID);
			switch (itemID){
				case 27: //VK-47 Flatline
				case 77: //R-301 Carbine
				case 171: //Shield (Level 3 / Purple)
				case 175: //Evo Shield (Level 3 / Purple)
				case 170: //Helmet (Level 4 / Gold)
				case 184:	//Backpack (Level 3 / Purple)
				case 185:  //Backpack (Level 4 / Gold)
				case 166: //Head Level 3 / Purple
				case 167: //Head Level 4 / Gold
			rx_write_i32(r5apex, entity + 0x2C4, 1512990053);			
			rx_write_i32(r5apex, entity + 0x3c8, 1);
			rx_write_i32(r5apex, entity + 0x3d0, 2);
			
			rx_write_float(r5apex, entity + 0x1D0, item_r);
			rx_write_float(r5apex, entity + 0x1D4, item_g);
			rx_write_float(r5apex, entity + 0x1D8, item_b);
			break;
			}
		} */
		/*for (int k = 0; k < 10000; k++)
		{	
			QWORD entity = GetClientEntity(r5apex, IClientEntityList, k);							
			if(IsButtonDown(r5apex, IInputSystem, 111))					
			{
			rx_write_i32(r5apex, entity + 0x02c0, 1363184265);
			rx_write_i32(r5apex, entity + 0x262, 16256);
			rx_write_i32(r5apex, entity + 0x2dc, 1193322764);
			}			
			}
			if(IsButtonDown(r5apex, IInputSystem, 110))
			{
			rx_write_i32(r5apex, entity + 0x02c0, 1411417991);
			rx_write_i32(r5apex, entity + 0x262, 0);
			rx_write_i32(r5apex, entity + 0x2dc, 0);			
			}									
		}	*/
		
		for (int i = 0; i < 70; i++)
		{
			QWORD entity = GetClientEntity(r5apex, IClientEntityList, i);
			/*
			float targetangle = rx_read_float(r5apex, entity + OFFSET_YAW);
      			float targetyaw = -targetangle; // yaw is inverted
      			if (targetyaw < 0)
           			 {
           			 targetyaw += 360;
           			 }
           		else
       				 {
       				 targetyaw += 90; // yaw is off by 90
       				 }
     			if (targetyaw > 360)
            			{
            			targetyaw -= 360;
            			}
      			float localangle = rx_read_float(r5apex, localplayer + OFFSET_YAW); 
      			float localyaw = -localangle; // yaw is inverted
       			if (localyaw < 0)
            			{
            			localyaw += 360;
            			}
        		else
        			{
        			localyaw += 90; // yaw is off by 90
        			}
        		if (localyaw > 360)
            			{
            			localyaw -= 360; 
            			}
       			if (targetyaw == localyaw && rx_read_i32(r5apex, entity + m_iHealth)  == 0)
           			{
           			spectatorcount++;
           			}
           		else		
       				{
       				spectatorcount = 0;
       				}
       			if (	spectatorcount > 0 && rx_read_int(r5apex, localplayer + m_iHealth) > 0)
       				{
       				printf("\r[+] Spectator: %d", spectatorcount);
				fflush(stdout);
				}
			else
				{
       				printf("\r[+] Spectator: 00");
				fflush(stdout);
				}
				*/
			
				
			int EntTeam = rx_read_i32(r5apex, entity + m_iTeamNum);
					if (EntTeam % 2) {
						iTeamControl = 1;
						}
					else {
						iTeamControl = 2;
						}
			int LocTeam = rx_read_i32(r5apex, localplayer + m_iTeamNum);
					if (LocTeam % 2) {
						iLocControl = 1;
						}
					else {
						iLocControl = 2;
						}
						
			
			if (gameMode==1953394531){
				if (iTeamControl == iLocControl)
				continue;
				}
						
			if (entity == 0)
				continue;

			if (entity == localplayer)
				continue;

			if (rx_read_i32(r5apex, entity + m_iHealth) == 0)
			{
				lastvis_aim[i] = 0;
				continue;
			}
			
		/*	if (rx_read_i32(r5apex, entity + m_iName) != 125780153691248)
			{
				continue;
			} */

			if (rx_read_i32(r5apex, entity + m_iTeamNum) == local_team)
			{
				continue;
			}
			
			//if (rx_read_i32(r5apex, entity + m_bleedoutState) == 0)
			//{
				//continue;
			//}

			if (rx_read_i32(r5apex, entity + m_lifeState) != 0)
			{
				lastvis_aim[i] = 0;
				continue;
			}
						
      
			vec3 head = GetBonePosition(r5apex, entity, 2);

			vec3 velocity;
			rx_read_process(r5apex, entity + m_vecAbsOrigin - 0xC, &velocity, sizeof(vec3));
			

			float fl_time = vec_distance(head, muzzle) / bulletSpeed;
			head.z += (700.0f * bulletGravity * 0.5f) * (fl_time * fl_time);

			velocity.x = velocity.x * fl_time;
			velocity.y = velocity.y * fl_time;
			velocity.z = velocity.z * fl_time;

			head.x += velocity.x;
			head.y += velocity.y;
			head.z += velocity.z;

			vec3 target_angle = CalcAngle(muzzle, head);
			vec3 breath_angles;

			rx_read_process(r5apex, localplayer + m_iViewAngles - 0x10, &breath_angles, sizeof(vec3));

			float last_visible = rx_read_float(r5apex, entity + dwVisibleTime);
			//glow enable
			rx_write_i32(r5apex, entity + 0x2C4, 1512990053);
			rx_write_i32(r5apex, entity + 0x3c8, 1);
			rx_write_i32(r5apex, entity + 0x3d0, 2);
			
			if (last_visible != 0.00f)
			{
		
				float fov = get_fov(breath_angles, target_angle);
	
				if (fov < target_fov && last_visible > lastvis_aim[i]) //i think this if is not working, always false
				{

					target_fov = fov;
					target_entity = entity;
					lastvis_aim[i] = last_visible;

					//luiz
					rx_write_float(r5apex, entity + 0x3B4, 99999999.0f); //glow distance
					
				if (rx_read_i32(r5apex, entity + 0x0170) <= 10){
					//cinza low life
					rx_write_float(r5apex, entity + 0x1D0, 176.0f);
					rx_write_float(r5apex, entity + 0x1D4, 176.0f);
					rx_write_float(r5apex, entity + 0x1D8, 176.0f);

				}else if (rx_read_i32(r5apex, entity + 0x0170) <= 50){
					//branco
					rx_write_float(r5apex, entity + 0x1D0, 255.0f);
					rx_write_float(r5apex, entity + 0x1D4, 255.0f);
					rx_write_float(r5apex, entity + 0x1D8, 255.0f);

				}else if(rx_read_i32(r5apex, entity + 0x0170) <= 75){
					//azul
					rx_write_float(r5apex, entity + 0x1D0, 0.0f);
					rx_write_float(r5apex, entity + 0x1D4, 117.0f);
					rx_write_float(r5apex, entity + 0x1D8, 209.0f);
				}else if(rx_read_i32(r5apex, entity + 0x0170) <= 100){
					//roxo
					rx_write_float(r5apex, entity + 0x1D0, 126.0f);
					rx_write_float(r5apex, entity + 0x1D4, 0.0f);
					rx_write_float(r5apex, entity + 0x1D8, 255.0f);
				}else if(rx_read_i32(r5apex, entity + 0x0170) <= 75){
					//vermelho
					rx_write_float(r5apex, entity + 0x1D0, 255.0f);
					rx_write_float(r5apex, entity + 0x1D4, 0.0f);
					rx_write_float(r5apex, entity + 0x1D8, 0.0f);
				}
				}
				else
				{

					rx_write_float(r5apex, entity + 0x3B4, 99999999.0f); //glow distance

				if (rx_read_i32(r5apex, entity + 0x0170) <= 10){
					//cinza low life
					rx_write_float(r5apex, entity + 0x1D0, 176.0f);
					rx_write_float(r5apex, entity + 0x1D4, 176.0f);
					rx_write_float(r5apex, entity + 0x1D8, 176.0f);

				}else if (rx_read_i32(r5apex, entity + 0x0170) <= 50){
					//branco
					rx_write_float(r5apex, entity + 0x1D0, 255.0f);
					rx_write_float(r5apex, entity + 0x1D4, 255.0f);
					rx_write_float(r5apex, entity + 0x1D8, 255.0f);

				}else if(rx_read_i32(r5apex, entity + 0x0170) <= 75){
					//azul
					rx_write_float(r5apex, entity + 0x1D0, 0.0f);
					rx_write_float(r5apex, entity + 0x1D4, 117.0f);
					rx_write_float(r5apex, entity + 0x1D8, 209.0f);
				}else if(rx_read_i32(r5apex, entity + 0x0170) <= 100){
					//roxo
					rx_write_float(r5apex, entity + 0x1D0, 126.0f);
					rx_write_float(r5apex, entity + 0x1D4, 0.0f);
					rx_write_float(r5apex, entity + 0x1D8, 255.0f);
				}else if(rx_read_i32(r5apex, entity + 0x0170) <= 75){
					//vermelho
					rx_write_float(r5apex, entity + 0x1D0, 255.0f);
					rx_write_float(r5apex, entity + 0x1D4, 0.0f);
					rx_write_float(r5apex, entity + 0x1D8, 0.0f);
				}
				}
			}
		}

		if (target_entity && IsButtonDown(r5apex, IInputSystem, AIMKEY))
		{

			if (rx_read_i32(r5apex, target_entity + m_iHealth) == 0)
				continue;
			if (rx_read_i32(r5apex, target_entity + m_bleedoutState) > 0) //ignore knock
				continue;

			//luiz - distancia
			vec3 enmPos;
			
			rx_read_process(r5apex, localplayer + m_vecAbsOrigin, &local_position, sizeof(vec3));
			rx_read_process(r5apex, target_entity + 0x158, &enmPos, sizeof(vec3)); //offset distance
			float distance = ((CalcDistance(local_position, enmPos)/100)*2); //need to verify
			printf("  	distance %f", ((CalcDistance(local_position, enmPos))/100)*2);
			bool far = (distance >= maxdistance);

			if(far){
				printf(" Cancelling ");
				continue;
			}

			printf(" Continue ");

			vec3 target_angle = {0, 0, 0};
			float fov = 360.0f;
			//luiz - alteracao hitbox
			//int bone_list[] = {2, 3, 5, 8};
			int bone_list[] = {37, 13, 7, 15, 9}; //chest

			vec3 breath_angles;
			rx_read_process(r5apex, localplayer + m_iViewAngles - 0x10, &breath_angles, sizeof(vec3));

			for (int i = 0; i < 4; i++)
			{
				vec3 head = GetBonePosition(r5apex, target_entity, bone_list[i]);

				vec3 velocity;
				rx_read_process(r5apex, target_entity + m_vecAbsOrigin - 0xC, &velocity, sizeof(vec3));

				


				float fl_time = vec_distance(head, muzzle) / bulletSpeed;

				head.z += (700.0f * bulletGravity * 0.5f) * (fl_time * fl_time);

				velocity.x = velocity.x * fl_time;
				velocity.y = velocity.y * fl_time;
				velocity.z = velocity.z * fl_time;

				head.x += velocity.x;
				head.y += velocity.y;
				head.z += velocity.z;

				vec3 angle = CalcAngle(muzzle, head);
				float temp_fov = get_fov(breath_angles, angle);
				if (temp_fov < fov)
				{
					fov = temp_fov;
					target_angle = angle;
				}
			}

			DWORD weapon_id = rx_read_i32(r5apex, localplayer + m_iWeapon) & 0xFFFF;
			QWORD weapon = GetClientEntity(r5apex, IClientEntityList, weapon_id - 1);
			float zoom_fov = rx_read_float(r5apex, weapon + m_playerData + 0xb8);

			if (rx_read_i8(r5apex, localplayer + m_bZooming))
			{
				fl_sensitivity = (zoom_fov / 90.0f) * fl_sensitivity;
			}

			if (fov <= AIMFOV)
			{

				vec3 angles;
				angles.x = breath_angles.x - target_angle.x;
				angles.y = breath_angles.y - target_angle.y;
				angles.z = 0;
				vec_clamp(&angles);

				float x = angles.y;
				float y = angles.x;
				x = (x / fl_sensitivity) / 0.022f;
				y = (y / fl_sensitivity) / -0.022f;

				float sx = 0.0f, sy = 0.0f;

				float smooth = AIMSMOOTH;

				DWORD aim_ticks = 0;

				if (smooth >= 1.0f)
				{
					if (sx < x)
						sx = sx + 1.0f + (x / smooth);
					else if (sx > x)
						sx = sx - 1.0f + (x / smooth);
					else
						sx = x;

					if (sy < y)
						sy = sy + 1.0f + (y / smooth);
					else if (sy > y)
						sy = sy - 1.0f + (y / smooth);
					else
						sy = y;
					aim_ticks = (DWORD)(smooth / 100.0f);
				}
				else
				{
					sx = x;
					sy = y;
				}

				if (qabs((int)sx) > 100)
					continue;

				if (qabs((int)sy) > 100)
					continue;

				DWORD current_tick = rx_read_i32(r5apex, IInputSystem + 0xcd8);
				if (current_tick - previous_tick > aim_ticks)
				{
					previous_tick = current_tick;
					typedef struct
					{
						int x, y;
					} mouse_data;
					mouse_data data;

					data.x = (int)sx;
					data.y = (int)sy;
					rx_write_process(r5apex, IInputSystem + 0x1DB0, &data, sizeof(data));
					std::this_thread::sleep_for(sleep);
				}
			}
		}

		
	}

	

ON_EXIT:
	rx_close_handle(r5apex);
}

