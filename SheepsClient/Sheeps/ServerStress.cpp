#include "pch.h"
#include "io.h"
#include "direct.h"
#include "ServerStress.h"

typedef int (*serverStress_cmd_cb) (HSOCKET hsock, ServerProtocol* proto, int cmdNO, cJSON* root);

std::map<HSOCKET, HCLIENTINFO>* StressClientMap;
std::mutex* StressClientMapLock = NULL;

std::map<std::string, t_file_update> updatefile;

int StressServerInit()
{
	StressClientMap = new std::map<HSOCKET, HCLIENTINFO>;
	StressClientMapLock = new std::mutex;
	return 0;
}

int ServerUnInit()
{
	return 0;
}

static int MsgResponse(HSOCKET hsock, int cmdNo, int retCode, const char* msg)
{
	char buf[128] = { 0x0 };
	int len = snprintf(buf, sizeof(buf), "{\"retCode\":%d,\"retMsg\":\"%s\"}", retCode, msg);
	t_stress_protocol_head head;
	head.cmdNo = cmdNo;
	head.msgLen = len + sizeof(t_stress_protocol_head);

	char sockbuf[256] = { 0x0 };
	memcpy(sockbuf, &head, sizeof(t_stress_protocol_head));
	memcpy(sockbuf + sizeof(t_stress_protocol_head), buf, len);
	IOCPPostSendEx(hsock, sockbuf, head.msgLen);
	LOG(slogid, LOG_DEBUG, "%s:%d [%d %d:%s]\r\n", __func__, __LINE__, len, cmdNo, msg);
	return 0;
}

static int MsgSend(HSOCKET sock, int cmdNo, char* data, int len)
{
	t_stress_protocol_head head;
	head.cmdNo = cmdNo;
	head.msgLen = len + sizeof(t_stress_protocol_head);

	IOCPPostSendEx(sock, (char*)&head, sizeof(t_stress_protocol_head));
	IOCPPostSendEx(sock, data, len);
	LOG(slogid, LOG_DEBUG, "%s:%d [%d %d:%s]\r\n", __func__, __LINE__, len, cmdNo, data);
	return 0;
}

static void getFiles(char* project_path, std::map<std::string, t_file_update>* files)
{
	intptr_t hFile = 0;
	struct _finddata_t fileinfo;

	char allfile[256] = { 0x0 };
	snprintf(allfile, sizeof(allfile), "%s\\*", project_path);

	char fullpath[256] = { 0x0 };
	if ((hFile = _findfirst(allfile, &fileinfo)) != -1)
	{
		do
		{
			memset(fullpath, 0, sizeof(fullpath));
			snprintf(fullpath, sizeof(fullpath), "%s\\%s", project_path, fileinfo.name);

			if ((fileinfo.attrib & _A_SUBDIR))
			{
				if (strcmp(fileinfo.name, ".") == 0 || strcmp(fileinfo.name, "..") == 0)
					continue;
				getFiles(fullpath, files);
			}
			else
			{
				char shortpath[128] = { 0x0 };
				snprintf(shortpath, sizeof(shortpath), ".\\%s", fullpath + strlen(DllPath));

				t_file_update info = { 0x0 };
				getfilemd5view(fullpath, (unsigned char*)info.fmd5, sizeof(info.fmd5));

				struct _stat64 finfo;
				_stat64(fullpath, &finfo);
				info.size = finfo.st_size;

				files->insert(std::pair<std::string, t_file_update>(shortpath, info));
			}
		} while (_findnext(hFile, &fileinfo) == 0);
		_findclose(hFile);
	}
}

static int sync_files(HSOCKET hsock)
{
	updatefile.clear();
	getFiles(ProjectPath, &updatefile);
	cJSON* root = cJSON_CreateObject();
	cJSON* array = cJSON_CreateArray();
	cJSON_AddItemToObject(root, "filelist", array);
	std::map<std::string, t_file_update>::iterator iter;
	for (iter = updatefile.begin(); iter != updatefile.end(); iter++)
	{
		cJSON* item = cJSON_CreateObject();
		cJSON_AddStringToObject(item, "File", iter->first.c_str());
		cJSON_AddNumberToObject(item, "Size", (const double)(iter->second.size));
		cJSON_AddStringToObject(item, "Md5", iter->second.fmd5);
		cJSON_AddItemToArray(array, item);
	}

	char* data = cJSON_PrintUnformatted(root);
	MsgSend(hsock, 9, data, (int)strlen(data));
	//LOG(slogid, LOG_DEBUG, "%s\r\n", data);
	sheeps_free(data);
	cJSON_Delete(root);
	return 0;
}

static int server_cmd_1_report_client_info(HSOCKET hsock, ServerProtocol* proto, int cmdNO, cJSON* root)
{
	cJSON* cpu = cJSON_GetObjectItem(root, "CPU");
	cJSON* projectid = cJSON_GetObjectItem(root, "ProjectID");
	if (cpu == NULL || projectid == NULL || cpu->type != cJSON_Number || projectid->type != cJSON_Number)
	{
		LOG(slogid, LOG_ERROR, "%s:%d error\r\n", __func__, __LINE__);
		return -1;
	}

	proto->stressInfo->ip = hsock->peer_ip;
	proto->stressInfo->port = hsock->peer_port;
	proto->stressInfo->cpu = cpu->valueint;
	proto->stressInfo->projectid = projectid->valueint;
	StressClientMapLock->lock();
	StressClientMap->insert(std::pair<HSOCKET, HCLIENTINFO>(hsock, proto->stressInfo));
	StressClientMapLock->unlock();

	MsgResponse(hsock, 1, 0, "OK");
	sync_files(hsock);
	return 0;
}

static int server_cmd_10_download_file(HSOCKET hsock, ServerProtocol* proto, int cmdNO, cJSON* root)
{
	LOG(slogid, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
	cJSON* file = cJSON_GetObjectItem(root, "File");
	cJSON* offset = cJSON_GetObjectItem(root, "Offset");
	cJSON* size = cJSON_GetObjectItem(root, "Size");
	if (file == NULL || offset == NULL || size == NULL)
	{
		LOG(slogid, LOG_ERROR, "%s:%d parm error\r\n", __func__, __LINE__);
		return -1;
	}
	char filepath[128] = { 0x0 };
	memcpy(filepath, file->valuestring, strlen(file->valuestring));
	if (cmdNO == 10)  //��ʱ����
	{
		char* s = filepath;
		char* p = NULL;
		while (true)
		{
			p = strchr(s, '/');
			if (p == NULL)
				break;
			*p = '\\';
			s = p + 1;
		}
	}//��ʱ����

	char* p = strchr(filepath, '\\');
	if (p == NULL)
	{
		LOG(slogid, LOG_ERROR, "%s:%d error\r\n", __func__, __LINE__);
		return -1;
	}

	char fullpath[256] = { 0x0 };
	snprintf(fullpath, sizeof(fullpath), "%s%s", DllPath, p + 1);
	FILE* hfile;
	int ret = fopen_s(&hfile, fullpath, "rb");
	if (ret != 0)
	{
		LOG(slogid, LOG_ERROR, "%s:%d open file error\r\n", __func__, __LINE__);
		return -1;
	}

#define FILEBUFLEN 10240
	int len = 0;
	int count = size->valueint / FILEBUFLEN;
	int last = size->valueint % FILEBUFLEN;
	char* res = NULL;

	char* buf = (char*)sheeps_malloc(FILEBUFLEN);
	char* base64 = (char*)sheeps_malloc(FILEBUFLEN);
	if (buf == NULL || base64 == NULL)
	{
		LOG(slogid, LOG_ERROR, "%s:%d memory error\r\n", __func__, __LINE__);
		goto end_free;
	}

	fseek(hfile, offset->valueint, SEEK_SET);

	for (int i = 0; i < count; i++)
	{
		fread(buf, sizeof(char), FILEBUFLEN, hfile);
		size_t n = base64_encode(buf, FILEBUFLEN, base64);
		//LOG(slogid, LOG_DEBUG, "base64=[%s]\r\n", base64);
		cJSON* data = cJSON_CreateObject();
		if (data == NULL)
		{
			LOG(slogid, LOG_ERROR, "%s:%d memory error\r\n", __func__, __LINE__);
			goto end_free;
		}
		cJSON_AddStringToObject(data, "File", filepath);
		cJSON_AddNumberToObject(data, "Offset", offset->valueint);
		cJSON_AddNumberToObject(data, "Order", i);
		cJSON_AddNumberToObject(data, "Size", FILEBUFLEN);
		cJSON_AddStringToObject(data, "Data", base64);
		cJSON_AddNumberToObject(data, "retCode", 0);
		cJSON_AddStringToObject(data, "retMsg", "OK");

		res = cJSON_PrintUnformatted(data);
		MsgSend(hsock, 10, res, int(strlen(res)));
		sheeps_free(res);
		memset(buf, 0, FILEBUFLEN);
		memset(base64, 0, FILEBUFLEN);
		cJSON_Delete(data);
		
	}
	if (last > 0)
	{
		fread(buf, sizeof(char), last, hfile);
		size_t n = base64_encode(buf, last, base64);
		//LOG(slogid, LOG_DEBUG, "base64=[%d:%s]\r\n", n, base64);
		cJSON* data = cJSON_CreateObject();
		if (data == NULL)
		{
			LOG(slogid, LOG_ERROR, "%s:%d memory error\r\n", __func__, __LINE__);
			goto end_free;
		}
		cJSON_AddStringToObject(data, "File", filepath);
		cJSON_AddNumberToObject(data, "Offset", offset->valueint);
		cJSON_AddNumberToObject(data, "Order", count);
		cJSON_AddNumberToObject(data, "Size", last);
		cJSON_AddStringToObject(data, "Data", base64);
		cJSON_AddNumberToObject(data, "retCode", 0);
		cJSON_AddStringToObject(data, "retMsg", "OK");

		res = cJSON_PrintUnformatted(data);
		//LOG(slogid, LOG_DEBUG, "%s\r\n", res);
		MsgSend(hsock, 10, res, int(strlen(res)));
		sheeps_free(res);
		memset(buf, 0, FILEBUFLEN);
		memset(base64, 0, FILEBUFLEN);
		cJSON_Delete(data);
	}

end_free:
	fclose(hfile);
	sheeps_free(buf);
	sheeps_free(base64);
	return 0;
}

static int server_cmd_11_sync_files_over(HSOCKET hsock, ServerProtocol* proto, int cmdNO, cJSON* root)
{
	std::map<HSOCKET, HCLIENTINFO>::iterator iter;
	iter = StressClientMap->find(hsock);
	if (iter != StressClientMap->end())
		iter->second->ready = true;
	return 0;
}

static int server_cmd_13_report_error(HSOCKET hsock, ServerProtocol* proto, int cmdNO, cJSON* root)
{
	return 0;
}

std::map<int, serverStress_cmd_cb> ServerFunc{
	{1,		server_cmd_1_report_client_info},
	{10,	server_cmd_10_download_file},
	{11,	server_cmd_11_sync_files_over},
	{13,	server_cmd_13_report_error}
};

static void do_server_func_by_cmd(HSOCKET hsock, ServerProtocol* proto, int cmdNO, cJSON* root)
{
	std::map<int, serverStress_cmd_cb>::iterator iter = ServerFunc.find(cmdNO);
	if (iter != ServerFunc.end())
		iter->second(hsock, proto, cmdNO, root);
}

int CheckStressRequest(HSOCKET hsock,ServerProtocol* proto , const char* data, int len)
{
	if (len < sizeof(t_stress_protocol_head))
		return 0;
	t_stress_protocol_head head;
	memcpy(&head, data, sizeof(t_stress_protocol_head));
	if (len < head.msgLen)
		return 0;
	int clen = head.msgLen - sizeof(t_stress_protocol_head);
	char* body = (char*)sheeps_malloc((size_t)clen + 1);
	if (body == NULL)
		return -1;
	memcpy(body, data + sizeof(t_stress_protocol_head), clen);
	LOG(slogid, LOG_DEBUG, "%s:%d [%d %d:%s]\r\n", __func__, __LINE__, head.msgLen, head.cmdNo, body);
	if (head.cmdNo == 10)  //��ʱ����
	{
		char* s = body;
		char* p = NULL;
		while (true)
		{
			p = strchr(s, '\\');
			if (p == NULL)
				break;
			*p = '/';
			s = p + 1;
		}
	}//��ʱ����
	cJSON* root = cJSON_Parse(body);
	if (root == NULL)
	{
		LOG(slogid, LOG_ERROR, "%s:%d json encoding error\r\n", __func__, __LINE__);
		sheeps_free(body);
		return -1;
	}
	do_server_func_by_cmd(hsock, proto, head.cmdNo, root);
	sheeps_free(body);
	cJSON_Delete(root);
	return head.msgLen;
}

void StressConnectionClosed(HSOCKET hsock, ServerProtocol* proto)
{
	StressClientMapLock->lock();
	StressClientMap->erase(proto->initSock);
	StressClientMapLock->unlock();
	proto->initSock = NULL;
	LOG(slogid, LOG_DEBUG, "%s:%d\r\n", __func__, __LINE__);
}