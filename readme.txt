#使用说明

linux环境需要安装g++   yum install gcc-c++


目录说明
/linux_mod  
linux内核模块编译目录
仅在内核版本2.6环境编译通过，  内核3.10.0-327.el7.x86_64编译失败

/linux_mod/test
linux内核模块编译目录
编译成功后 运行./test [count]
cat /proc/myhash/default_table 可查看默认表状态

/linux
linux环境测试编译目录
编译成功后 运行./test [count]

运行结果：

[root@localhost linux]# ./test  300000
set start_size :300000
===============begin==============
insert cost: 0.898906s
  find cost: 0.078865s
 erase cost: 0.080299s
stat: thread:1, count:300000, lost:0 // 1个线程
=============== end ==============
===============begin==============
insert cost: 0.032309s
  find cost: 0.014421s
 erase cost: 0.020981s
stat: thread:4, count:300000, lost:0 // 4个线程
=============== end ==============
vmalloc:6,  vfree:6, kmalloc:1, kfree:1
stat: m_fail:0, i_fail:0: i_conf:0, ei_fail:0