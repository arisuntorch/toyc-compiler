from pathlib import Path

from docx import Document
from docx.enum.section import WD_SECTION
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor


ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "docs" / "practice_report.docx"


def set_east_asia_font(run, font_name):
    run.font.name = font_name
    run._element.rPr.rFonts.set(qn("w:eastAsia"), font_name)


def set_style_font(style, font_name, size_pt=None, color=None, bold=None):
    font = style.font
    font.name = font_name
    style.element.rPr.rFonts.set(qn("w:eastAsia"), font_name)
    if size_pt is not None:
        font.size = Pt(size_pt)
    if color is not None:
        font.color.rgb = RGBColor.from_string(color)
    if bold is not None:
        font.bold = bold


def set_para_spacing(style, before=0, after=6, line=1.10):
    pf = style.paragraph_format
    pf.space_before = Pt(before)
    pf.space_after = Pt(after)
    pf.line_spacing = line


def set_cell_shading(cell, fill):
    tc_pr = cell._tc.get_or_add_tcPr()
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), fill)
    tc_pr.append(shd)


def set_cell_text(cell, text, bold=False):
    cell.text = ""
    p = cell.paragraphs[0]
    p.alignment = WD_ALIGN_PARAGRAPH.LEFT
    run = p.add_run(text)
    set_east_asia_font(run, "Calibri")
    run.font.size = Pt(10.5)
    run.bold = bold


def add_heading(doc, text, level):
    p = doc.add_heading(text, level=level)
    return p


def add_body(doc, text):
    p = doc.add_paragraph(text)
    p.style = doc.styles["Normal"]
    return p


def add_bullet(doc, text):
    p = doc.add_paragraph(style="List Bullet")
    run = p.add_run(text)
    set_east_asia_font(run, "Calibri")
    run.font.size = Pt(10.5)
    return p


def add_numbered(doc, text):
    p = doc.add_paragraph(style="List Number")
    run = p.add_run(text)
    set_east_asia_font(run, "Calibri")
    run.font.size = Pt(10.5)
    return p


def add_metadata_table(doc):
    table = doc.add_table(rows=4, cols=2)
    table.alignment = WD_TABLE_ALIGNMENT.LEFT
    table.style = "Table Grid"
    widths = [Inches(1.5), Inches(5.0)]
    rows = [
        ("课程", "编译系统实践"),
        ("题目", "ToyC 到 RISC-V32 汇编编译器"),
        ("姓名/学号", "【请填写】"),
        ("提交仓库", "https://github.com/arisuntorch/toyc-compiler.git"),
    ]
    for row, (k, v) in zip(table.rows, rows):
        row.cells[0].width = widths[0]
        row.cells[1].width = widths[1]
        for cell in row.cells:
            cell.vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
        set_cell_shading(row.cells[0], "F2F4F7")
        set_cell_text(row.cells[0], k, bold=True)
        set_cell_text(row.cells[1], v)
    doc.add_paragraph()


def build():
    doc = Document()
    sec = doc.sections[0]
    sec.top_margin = Inches(1)
    sec.bottom_margin = Inches(1)
    sec.left_margin = Inches(1)
    sec.right_margin = Inches(1)
    sec.header_distance = Inches(0.492)
    sec.footer_distance = Inches(0.492)

    styles = doc.styles
    set_style_font(styles["Normal"], "Calibri", 10.5, "000000")
    set_para_spacing(styles["Normal"], before=0, after=6, line=1.10)
    set_style_font(styles["Heading 1"], "Calibri", 16, "2E74B5", True)
    set_para_spacing(styles["Heading 1"], before=16, after=8, line=1.10)
    set_style_font(styles["Heading 2"], "Calibri", 13, "2E74B5", True)
    set_para_spacing(styles["Heading 2"], before=12, after=6, line=1.10)
    set_style_font(styles["Heading 3"], "Calibri", 12, "1F4D78", True)
    set_para_spacing(styles["Heading 3"], before=8, after=4, line=1.10)
    set_style_font(styles["List Bullet"], "Calibri", 10.5, "000000")
    set_para_spacing(styles["List Bullet"], before=0, after=6, line=1.167)
    set_style_font(styles["List Number"], "Calibri", 10.5, "000000")
    set_para_spacing(styles["List Number"], before=0, after=6, line=1.167)

    title = doc.add_paragraph()
    title.alignment = WD_ALIGN_PARAGRAPH.CENTER
    run = title.add_run("编译系统实践报告")
    set_east_asia_font(run, "Calibri")
    run.font.size = Pt(18)
    run.font.bold = True
    run.font.color.rgb = RGBColor.from_string("0B2545")
    title.paragraph_format.space_after = Pt(12)

    subtitle = doc.add_paragraph()
    subtitle.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = subtitle.add_run("ToyC 到 RISC-V32 汇编编译器")
    set_east_asia_font(r, "Calibri")
    r.font.size = Pt(12)
    r.font.color.rgb = RGBColor.from_string("555555")
    subtitle.paragraph_format.space_after = Pt(18)

    add_metadata_table(doc)

    add_heading(doc, "1. 项目概述", 1)
    add_body(
        doc,
        "本项目实现了一个 ToyC 到 RISC-V32 汇编的编译器。编译器从标准输入读取 ToyC 源程序，"
        "经过词法分析、语法分析、语义环境维护和代码生成，最终向标准输出写出 RISC-V32 汇编程序。"
    )

    add_heading(doc, "2. 总体架构", 1)
    add_body(doc, "编译器采用 C++20 实现，主要模块集中在 src/main.cpp：")
    for item in [
        "Lexer：识别关键字、标识符、整数、运算符、分隔符，并跳过空白和注释。",
        "Parser：使用递归下降分析 ToyC 文法，构造抽象语法树。",
        "AST：表示声明、函数、语句和表达式。",
        "CodeGen：维护符号表、作用域、常量值、栈帧布局，并生成 RISC-V32 汇编。"
    ]:
        add_bullet(doc, item)

    add_heading(doc, "3. 词法分析", 1)
    add_body(
        doc,
        "词法分析器支持 ToyC 语言中的关键字、标识符、十进制整数、算术运算符、关系运算符、"
        "逻辑运算符、括号、花括号、分号和逗号。空白字符、单行注释和多行注释会被忽略。"
    )

    add_heading(doc, "4. 语法分析", 1)
    add_body(doc, "语法分析器采用递归下降方法。表达式部分按照优先级分层处理：")
    for item in [
        "逻辑或表达式。",
        "逻辑与表达式。",
        "关系和相等性表达式。",
        "加减表达式。",
        "乘除模表达式。",
        "一元表达式。",
        "基本表达式。"
    ]:
        add_numbered(doc, item)
    add_body(doc, "这种结构直接对应 ToyC 文法，便于处理运算符优先级和左结合性。")

    add_heading(doc, "5. 语义处理", 1)
    add_body(
        doc,
        "编译器维护嵌套作用域符号表，用于区分局部变量、全局变量和常量。常量声明会在编译期求值，"
        "后续引用直接替换成立即数。局部变量和函数形参分配在函数栈帧中，全局变量分配在 .data 段。"
    )

    add_heading(doc, "6. 代码生成", 1)
    add_body(doc, "代码生成目标为 RISC-V32 汇编。每个函数建立独立栈帧：")
    for item in [
        "ra 和旧 s0 保存在栈帧顶部。",
        "s0 作为帧指针。",
        "参数和局部变量使用相对 s0 的偏移访问。",
        "表达式结果统一放入 a0。"
    ]:
        add_bullet(doc, item)
    add_body(
        doc,
        "函数调用按照 RISC-V 调用习惯处理，前八个参数放入 a0 到 a7，更多参数放到调用者栈上。"
    )

    add_heading(doc, "7. 控制流", 1)
    add_body(
        doc,
        "if-else 和 while 使用自动生成的局部标签和条件跳转实现。break 跳转到当前循环结束标签，"
        "continue 跳转到当前循环条件判断标签。逻辑与和逻辑或使用短路求值生成，不会无条件计算右侧表达式。"
    )

    add_heading(doc, "8. 优化实现", 1)
    add_body(
        doc,
        "当前实现以功能正确性为优先目标，同时实现了常量表达式折叠和部分立即数加减法优化。"
        "常量表达式在编译阶段求值，可以减少运行时指令数量；常见的变量加减小立即数会生成 addi 指令，"
        "避免不必要的压栈和二元运算序列。"
    )

    add_heading(doc, "9. 测试", 1)
    add_body(doc, "项目包含三个基础测试：")
    for item in [
        "tests/basic.tc：测试全局变量、常量、算术表达式。",
        "tests/control.tc：测试循环、分支、break、continue 和逻辑表达式。",
        "tests/functions.tc：测试递归函数和超过八个参数的函数调用。"
    ]:
        add_bullet(doc, item)
    add_body(doc, "本地使用 make clean && make && make test 完成构建和样例汇编生成验证。")

    add_heading(doc, "10. 后续优化方向", 1)
    for item in [
        "更完整的常量传播。",
        "死代码删除。",
        "公共子表达式消除。",
        "更好的寄存器分配。"
    ]:
        add_bullet(doc, item)

    add_heading(doc, "11. 总结", 1)
    add_body(
        doc,
        "本项目完成了 ToyC 编译器的基本前端和后端，实现了从源程序到 RISC-V32 汇编的完整编译流程。"
        "整体实现以正确性和可维护性为优先目标，为后续性能优化预留了空间。"
    )

    doc.save(OUT)


if __name__ == "__main__":
    build()
    print(OUT)
