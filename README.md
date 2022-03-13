# **FileBatcher文件分批**

> 功能为将某一目录下的大量文件分批，每批个数可以指定。  
> 使用多线程处理（最多16线程），可自定义分批文件夹名称。  
>> 分批文件夹名称由规则指定，可在rules文件夹下导入自己的规则。  
>> 规则文件为lua脚本，脚本须包含一个名为"title"的字符串作为规则名，  
>> 一个名为"helptext"的字符串作为规则的说明文档，  
>> 以及一个名为"settext"的函数作为规则主体。  
>> settext函数必须传入两个整数“m”，“n”返回一个字符串，  
>> 其中"m"为第几个文件夹，"n"为每批的文件数。  
>> 程序运行时会执行符合规则的脚本  
> 使用VS2019进行编译，编译后将编译得到的程序文件和两个Lua的DLL文件及rules文件夹放至同一文件夹下即可运行。  
