学习llvm
---------------------------

参考:https://llvm.org/docs/tutorial/MyFirstLanguageFrontend/index.html
# chapter1 and 2 参考chapt_1_2.cpp 详细注释了从词法,语法,语义,ast的理解
# chapter3 IR:需要识别后端target
# chapter4 ==pass jit==
```
pass:constant fold
void InitializeModuleAndPassManager(void) {
  // Open a new module.
  TheModule = std::make_unique<Module>("my cool jit", TheContext);

  // Create a new pass manager attached to it.
  TheFPM = std::make_unique<FunctionPassManager>(TheModule.get());

  // Do simple "peephole" optimizations and bit-twiddling optzns.
  TheFPM->add(createInstructionCombiningPass());
  // Reassociate expressions.
  TheFPM->add(createReassociatePass());
  // Eliminate Common SubExpressions.
  TheFPM->add(createGVNPass());
  // Simplify the control flow graph (deleting unreachable blocks, etc).
  TheFPM->add(createCFGSimplificationPass());

  TheFPM->doInitialization();
}
```
'jit
JIT (Just-In-Time - 实时编译) 和 AOT (Ahead-Of-Time - 预先编译)
- JIT：
  吞吐量高，有运行时性能加成，可以跑得更快，并可以做到动态生成代码等，但是相对启动速度较慢，并需要一定时间和调用频率才能触发 JIT 的分层机制,JIT编译器可以经过准确调节达到当前运行时状态，结果可以完成一些预编译语言无法完成的工作：更高效地利用和分配CPU寄存器。在适当的情况下实施低级代码优化，比如常量重叠、拷贝复制、取消范围检查、取消常规副表达式以及方法内联等
- AOT：
是事先生成机器码，其实就跟C++这样的语言差不多。选择这么做通常都会意味着你损失了一个功能——譬如说
  * C#的【虚函数也可以是模板函数】功能啦；
  用反射就地组合成新模板类（你有List<>，有int，代码里面没出现过List<int>，你也可以new出来，C++怎么做都不行的）】功能啦；
  * 内存占用低，启动速度快，可以无需 runtime 运行，直接将 runtime 静态链接至最终的程序中，但是无运行时性能加成，不能根据程序运行情况做进一步的优化,所有这些功能都要求你必须运行到那才产生机器码的。JIT还有一个好处就是做profiling based optimization方便。当然，这样就使得运行的时候会稍微慢一点点，不过这一点点是人类不可察觉的。
'

		