#include <Python.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>

#define IP "127.0.0.1"
#define PORT 8011
const int BUFFER_LEN = 1024;

void print_err(char *str, int line, int err_no) {
	printf("%d, %s :%s\n", line, str, strerror(err_no));
	_exit(-1);
}

PyObject* call_overlay_construct(PyObject *class_obj)
{
    PyObject *args;
    PyGILState_STATE state = PyGILState_Ensure();

    args = Py_BuildValue("(s)", "base.bit");
    PyObject *obj = PyObject_CallObject(class_obj, args);

    Py_DECREF(args);

    return obj;
}

PyObject *import_name(const char *modname, const char *symbol)
{
    PyObject *u_name, *module;
    u_name = PyUnicode_FromString(modname);
    module = PyImport_Import(u_name);
    Py_DECREF(u_name);

    return PyObject_GetAttrString(module, symbol);
}

int main()
{
    PyObject *overlay_func;
    PyObject *base;
    PyObject *pAudio;

    clock_t start, end;

    Py_Initialize();
    
    printf("PYNQ初始化\n");
    start = clock();
    overlay_func = import_name("pynq.overlays.base", "BaseOverlay");
    base = call_overlay_construct(overlay_func);
    end = clock();
    printf("加载完成，耗时=%lfs\n", (double)(end - start) / CLOCKS_PER_SEC);

    pAudio = PyObject_GetAttrString(base, "audio");
    PyObject *record_method = PyObject_GetAttrString(pAudio, "record");
    PyObject *save_method = PyObject_GetAttrString(pAudio, "save");

    int skfd = -1, ret = -1;
    skfd = socket(AF_INET, SOCK_STREAM, 0);
    if (skfd < 0) {
	print_err("socket failed", __LINE__, errno);
        return 0;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(IP);
    	
    ret = connect(skfd,(struct sockaddr*)&addr, sizeof(addr));
    if(ret < 0) print_err("connect failed", __LINE__, errno);

    int n = -1;
    //char filename[50];
    while(1) {
        printf("\n输入命令(1:录音识别,0:退出)\n");
        scanf("%d", &n);
        if(n == 0) break;
        
        //printf("输入文件名(.wav): ");
        //scanf("%s", filename);
        const char* filename = "ytkj.wav";
        printf("=====开始录音=====\n");
        PyObject *record_time = Py_BuildValue("(i)", 3);
        start = clock();
        PyObject_CallObject(record_method, record_time);
        end = clock();
        printf("录音时间 = %lfs\n", (double)(end - start) / CLOCKS_PER_SEC);
        printf("=====录音结束=====\n\n");

        printf("=====开始转换=====\n");
        PyObject *file_name = Py_BuildValue("(s)", filename);
        start = clock();
        PyObject_CallObject(save_method, file_name);
        end = clock();
        printf("转换耗时 = %lfs\n", (double)(end - start) / CLOCKS_PER_SEC);
        printf("=====转换结束=====\n\n");

        char buf[BUFFER_LEN];
        char rec[BUFFER_LEN];
        bzero(&buf, sizeof(buf));
        getcwd(buf, sizeof buf);
        strcat(buf, "/");
        strcat(buf, filename);
        printf("录音文件路径: %s\n", buf);
        
        ret = send(skfd, &buf, sizeof(buf), 0);
        if (ret < 0) {
            print_err("send failed", __LINE__, errno);
        }
        bzero(&rec, sizeof(rec));
        ret = recv(skfd, &rec, sizeof(rec), 0);
        if(ret < 0) print_err("recv failed", __LINE__, errno);
        else if(ret > 0) printf("recv from server %s\n", rec);
	}

    Py_DECREF(overlay_func);
    Py_DECREF(base);
    Py_DECREF(pAudio);
    Py_Finalize();
    return 0;
}
