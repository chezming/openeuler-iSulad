# 方案目标
在Image Load & pull过程中，显示多个layer下载的进度。

之前的grpc pull和cri pull共用了接口，需要新增grpc pull接口，该接口类型为stream，带progress status。
重写函数oci_do_pull_image，底层函数pull_image复用。
在结构体registry_pull_options增加map。

# 限制
1. 每一个connection只做一件事，否则progress store会混乱。
2. 这个功能只为grpc 连接服务。

# 总体设计
## 主要功能模块
### Progress status store
为每一个connection建立一个status map。 map的key为Layer ID，内容结构体定义如下:

```
struct progress_status {
    // Layer ID
    char ID[13];

    // Progress of an action, such as Downloading, Loading
    char action[32];

    // total is the end value describing when we made 100% progress for an operation.
    int64 total;

    // start is the initial value for the operation.
    int64 start;

    // current is the current value for the operation.
    int64 current;
    
    // units is the unit to print for progress. It defaults to "B" - bytes if empty.
    char units[4];
}
```

#### API
```
map *init_progress_store();

int write_progress_status(map *, progress_status); 

map *get_progress_status();
```
### 状态设置
一般下载状态有:
- Pulling fs layer
- Waiting
- Downloading
- Downloaded
- Extract 
- Pull complete

还有一些异常状态，如Already exist，Retrying，所以该状态不做强制定义。

加载状态有:
- Loading layer

### Client Progress 显示
在client每次读到消息时，获取当前窗口宽度(termios.h: tcgetattr)，如果宽度小于110字符，则压缩显示(已下载/全部字节)，如果不是，则显示进度条。
当第一次收到时，计算需要显示的任务数task number，每个任务显示一行。
当更新状态时，将光标回退task number行，清除该行，打印完一行，将光标移到下一行清除该行并打印新的进度，重复上述步骤直至所有任务打印完成。

## 主要流程
### 下载任务获取下载状态
在结构体pull_descriptor新增outputfunc和outputparam， 传递write_progress_status和map *。
新增http_request_with_status封装http_request，兼容原有函数，增加参数outputfunc和param做output。
```
int http_request_with_status(const char *url, struct http_get_options *options, long *response_code, int recursive_len, void *outputFunc(void *), void *output_param);
```
在http_request_with_status中，新建线程，在curl_easy_perform之后，每隔100ms获取更新一次本任务状态并更新progress status map。

```
curl_off_t dl;
curl_easy_getinfo(curl， CURLINFO_SIZE_DOWNLOAD_T， &dl);
```

### server获取下载状态并传递给client
新增函数int ImagesServiceImpl::Pull， oci_pull_with_status。 在pull_image里新建线程，初始化status map，在结构体参数options中传递。
每隔100ms读取status map并序列化为json message，写入response stream。

### client收取状态并显示
阻塞式读取response stream， 流不为空则一直读取并显示。
