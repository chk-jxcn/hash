#ʹ��˵��

linux������Ҫ��װg++   yum install gcc-c++


Ŀ¼˵��
/linux_mod  
linux�ں�ģ�����Ŀ¼
�����ں˰汾2.6��������ͨ����  �ں�3.10.0-327.el7.x86_64����ʧ��

/linux_mod/test
linux�ں�ģ�����Ŀ¼
����ɹ��� ����./test [count]
cat /proc/myhash/default_table �ɲ鿴Ĭ�ϱ�״̬

/linux
linux�������Ա���Ŀ¼
����ɹ��� ����./test [count]

���н����

[root@localhost linux]# ./test  300000
set start_size :300000
===============begin==============
insert cost: 0.898906s
  find cost: 0.078865s
 erase cost: 0.080299s
stat: thread:1, count:300000, lost:0 // 1���߳�
=============== end ==============
===============begin==============
insert cost: 0.032309s
  find cost: 0.014421s
 erase cost: 0.020981s
stat: thread:4, count:300000, lost:0 // 4���߳�
=============== end ==============
vmalloc:6,  vfree:6, kmalloc:1, kfree:1
stat: m_fail:0, i_fail:0: i_conf:0, ei_fail:0