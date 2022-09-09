<div align="center"><image width="140em" src="https://user-images.githubusercontent.com/66859419/183120498-1dede5b4-0666-4891-b95f-c3a812b3f12f.png" /></div>
<h1 align="center">BetterNCM II</h1>
<h3 align="center">PC 版网易云客户端插件管理器</h3>
<h3 align="center">使用 Rust 重写了的版本</h3>

> **本程序仅供学习用途，请勿用于非法用途！**

![image](https://user-images.githubusercontent.com/66859419/186859984-ac64b338-d649-410f-a156-8f7d676bc7a9.png)

## 使用

将 `msimg32.dll` 与原版网易云客户端安装目录中的同名文件替换即可，为了方便还原/卸载你可以提前备份该文件。

## 编译

先构建 `better-ncm-framework`，进入目录，安装好 Yarn，输入以下指令构建：

```bash
yarn
yarn build
```

然后开始构建本体，务必使用 `nightly` 频道的 Rust 编译器编译，编译目标为 `i686-pc-windows-msvc`

如果使用调试构建，则将会同时显示控制台方便查看输出，同时开启针对部分脚本的刷新重载，方便前端调试，发行构建则会去除这些特性。
