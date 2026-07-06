# 提交流程

## 已完成

- 本地项目已创建。
- 本地 Git 提交已创建。
- GitHub 私有仓库已创建并推送。

## 评测系统填写

仓库地址：

```text
https://github.com/arisuntorch/toyc-compiler.git
```

分支名：

```text
main
```

访问令牌：

不要把令牌发到聊天里。需要提交时，在本机终端运行：

```sh
gh auth token
```

把输出的 token 复制到评测系统的“访问令牌”输入框。

如果评测系统要求把 token 写进仓库地址，格式是：

```text
https://arisuntorch:你的token@github.com/arisuntorch/toyc-compiler.git
```

## 本地复验命令

```sh
cd /mnt/c/Users/arisu/OneDrive/文档/work
make clean
make
make test
```

## 出错时怎么处理

如果评测系统失败，把失败页面里的以下内容复制回来：

- 构建日志。
- 第一个失败测试点的错误信息。
- 如果有汇编器错误，复制报错行号和报错指令。

不要只发“失败了”，因为需要具体日志才能定位。
