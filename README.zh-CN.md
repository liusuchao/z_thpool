<div align="center">
  <img alt="z_thpool" width="120" height="120" src="./assets/logo/logo_1000.png">
  <h1>z_thpool</h1>
  <span>中文 | <a href="./README.md">English</a></span>
</div>

<div align="center">
  <br/>
  <a href="" target="_blank"><img src="https://abroad.hellogithub.com/v1/widgets/recommend.svg?rid=9433615761f548cf9648434c670cd85b&claim_uid=249cPWvjfNmU7dp" alt="Featured｜HelloGitHub" style="width: 250px; height: 54px;" width="250" height="54" /></a>
</div>

## ⚡ 介绍

**这是一个简单的 Linux 线程池。~** 😊

## 💻 图示
内部结构由线程池和循环队列组成。当线程池满时，新任务会被放入循环队列中。一旦有线程退出，就会从队列中取出一个任务进行处理。
<div align="center">
  <img alt="z_thpool" src="./assets/logic_block_diagram.png">
</div>

## 🚀 如何使用？

**下述操作基于当前项目的根目录，请注意以确保操作正确无误！**

### **编译**

```bash
cd z_thpool
make
cd lib
ls
libtestlib.a  libz_thpool.a   //编译完成
```

### **测试**

#### 完成测试
```bash
 ./build/z_thpool_test 
Enter a command (type 'exit' to quit):
> help  //输入help查看测试命令

Enter a command (type 'exit' to quit):

start 5 100 64  #Start with 10 threads, 50 cache queues, set the thread stack 64k size.
stop            #Stop. 
add 10          #Add 10 threads 
test            #Test thpool 
show            #Show thpool state 
help            #Help 
> test  // 输入test查完整测试情况
Starting thread pool extreme test...
[debug ][src/z_thpool.c:182, z_thpool_start] enter
[debug ][src/z_thpool.c:229, z_thpool_start] exit :0
Task 0 added successfully
...
Task 99 added successfully
Waiting...
Task 0 is being processed by thread 140675616458496
...
Task 99 is being processed by thread 140675609564928
Waiting...
Waiting...
Waiting...
Waiting...
Task 0 argument after processing: 0
...
Task 99 argument after processing: 99
Stopping thread pool
Thread pool stopped
Thread pool extreme test completed.
```
#### 单步测试
```bash
./build/z_thpool_test 
Enter a command (type 'exit' to quit):
> start 5 100 64         // 创建5个线程到线程池，创建100个缓存，每个线程是64K的栈空间
> add 100                // 添加100个线程
> show                   // 查看线程池状态
------------------------------------------------------------------------------------------------------------------------
| z_thpool module                                                                                                                                          
------------------------------------------------------------------------------------------------------------------------
Ver:               0.0.1.0
------------------------------------------------------------------------------------------------------------------------
max nums:          5     // 最大线程是5个
create nums:       5     // 已经创建线程是5个
busy nums:         5     // 正在运行的线程是5个
max cache nums:    100   // 创建的缓存列队是100个
use cache nums:    85    // 已经使用了85个，説明85个线程在列队中等待
pub_bytes:         1600
sub_bytes:         240
> show
------------------------------------------------------------------------------------------------------------------------
| z_thpool module                                                                                                                                          
------------------------------------------------------------------------------------------------------------------------
Ver:               0.0.1.0
------------------------------------------------------------------------------------------------------------------------
max nums:          5     // 最大线程是5个
create nums:       5     // 已经创建线程是5个
busy nums:         5     // 正在运行的线程是5个
max cache nums:    100   // 创建的缓存列队是100个
use cache nums:    85    // 已经使用了80个，説明80个线程在列队中等待。随着线程的处理，队列中的任务数量将逐渐减少，直至清空。
pub_bytes:         1600
sub_bytes:         320
> 
```


### **使用函数**
只需要三步
```c
//第一步：开启线程池
struct z_thpool_config_struct t_conftg;
t_conftg.max_thread_nums = 50;           // 同时运行的线程最大数量为50个
t_conftg.msg_node_max = 1000;            // 缓存队列的最大容量为1000个任务
t_conftg.thread_stack_size = 64 * 1024;  // 每个线程的栈大小为64KB
z_thpool_start(&t_conftg);

//第二步：添加任务到线程池
z_thpool_add_work(test_task_cb, &i);

//第三步：关闭线程池
//如果不再使用线程池，可以关闭它：
z_thpool_exit();
```
### **文件説明**
关键文件只有z_kfifo.c 和 z_thpool.c
```base
test.c      #测试程序
z_kfifo.c   #缓存队列
z_thpool.c  #线程池实现
z_debug.h   #信息打开
z_tool.h    #工具宏
z_table_print.c #用于测试程序的show命令,为了打印方便的打印表格
```

## 🛠️ 关于

## ❓ 常见问题

## 🤝开发指南 

## 🚀 Star 

## 🌟 贡献
感谢所有已经对 z_thpool 做出贡献的人！🎉
## 🌟 致谢
**感谢您花时间阅读我们的项目文档。**
**如果您觉得这个项目对您有帮助，请帮忙Star支持我们，谢谢！**
