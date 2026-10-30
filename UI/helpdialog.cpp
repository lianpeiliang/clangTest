#include "helpdialog.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFrame>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

QLabel *textLabel(const QString &text, const char *name, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setObjectName(QLatin1String(name));
    label->setTextFormat(Qt::PlainText);
    label->setWordWrap(true);
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
    return label;
}

QVBoxLayout *addPage(QTabWidget *tabs, const QString &title)
{
    auto *scroll = new QScrollArea(tabs);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *page = new QWidget(scroll);
    page->setObjectName(QStringLiteral("helpPage"));
    auto *layout = new QVBoxLayout(page);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(12);
    scroll->setWidget(page);
    tabs->addTab(scroll, title);
    return layout;
}

void addCard(QVBoxLayout *layout, const QString &title, const QString &body)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("helpCard"));
    auto *contents = new QVBoxLayout(card);
    contents->setContentsMargins(14, 12, 14, 12);
    contents->setSpacing(6);
    contents->addWidget(textLabel(title, "helpSectionTitle", card));
    contents->addWidget(textLabel(body, "helpBody", card));
    layout->addWidget(card);
}

// 使用布局控件画流程，文字可选择，缩放后仍保持清晰且不会依赖外部图片。
void addFlow(QVBoxLayout *layout, const QString &title, const QStringList &steps)
{
    auto *card = new QFrame;
    card->setObjectName(QStringLiteral("helpCard"));
    auto *contents = new QVBoxLayout(card);
    contents->setContentsMargins(14, 12, 14, 12);
    contents->setSpacing(10);
    contents->addWidget(textLabel(title, "helpSectionTitle", card));
    auto *row = new QHBoxLayout;
    row->setSpacing(8);
    for (int i = 0; i < steps.size(); ++i) {
        if (i) {
            auto *arrow = textLabel(QStringLiteral("→"), "helpFlowArrow", card);
            arrow->setAlignment(Qt::AlignCenter);
            row->addWidget(arrow);
        }
        auto *node = textLabel(steps.at(i), "helpFlowNode", card);
        node->setAlignment(Qt::AlignCenter);
        node->setMinimumWidth(100);
        row->addWidget(node, 1);
    }
    contents->addLayout(row);
    layout->addWidget(card);
}

} // namespace

void showHelpDialog(QWidget *owner)
{
    if (auto *existing = owner->findChild<QDialog *>(QStringLiteral("helpDialog"))) {
        existing->showNormal();
        existing->raise();
        existing->activateWindow();
        return;
    }

    auto *dialog = new QDialog(owner);
    dialog->setObjectName(QStringLiteral("helpDialog"));
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setWindowTitle(QStringLiteral("StructHex · 使用帮助"));
    dialog->setMinimumSize(640, 420);
    const QScreen *screen = QGuiApplication::screenAt(owner->mapToGlobal(owner->rect().center()));
    if (!screen)
        screen = QGuiApplication::primaryScreen();
    const QSize available = screen ? screen->availableGeometry().size() : QSize(1024, 768);
    dialog->resize(qMin(880, available.width() - 60), qMin(760, available.height() - 100));

    auto *layout = new QVBoxLayout(dialog);
    layout->setContentsMargins(20, 20, 20, 16);
    layout->setSpacing(12);
    layout->addWidget(textLabel(QStringLiteral("认识 StructHex"), "helpTitle", dialog));
    layout->addWidget(textLabel(QStringLiteral("从结构定义到十六进制数据，直观看懂字段与内存布局。"),
                                "helpSubtitle", dialog));

    auto *tabs = new QTabWidget(dialog);
    tabs->setObjectName(QStringLiteral("helpTabs"));
    layout->addWidget(tabs, 1);

    auto *overview = addPage(tabs, QStringLiteral("功能概览"));
    addCard(overview, QStringLiteral("01  加载定义，查看布局"),
        QStringLiteral("打开 C/C++ 头文件，自动发现其中的结构体并从下拉框选择；也可以加载本程序导出的结构定义 JSON。"
                       "成员树展示嵌套字段、类型、大小、偏移及对齐，支持展开与折叠。修改源文件后点击“重新加载”。"));
    addCard(overview, QStringLiteral("02  编辑字段，生成数据"),
        QStringLiteral("在可编辑字段中输入十进制或 0x 前缀的十六进制整数，也可编辑布尔和浮点数。"
                       "程序会校验格式与数值范围；输入有效后，点击“查看16进制”，预览、复制或导出十六进制文本。"));
    addCard(overview, QStringLiteral("03  解析数据，填回编辑"),
        QStringLiteral("“解析十六进制”可选择小端或大端，先预览字段结果，再点击“填入可编辑字段”。"
                       "“导入16进制数据”及“从文件导入16进制数据”使用小端序，校验成功后直接更新字段；导入失败时保留原值。"));
    addCard(overview, QStringLiteral("04  保存定义，重复使用"),
        QStringLiteral("“导出结构定义 JSON”保存当前结构体的字段与布局，便于下次加载。"
                       "定义 JSON 不包含字段输入值；如需保存本次数据，请在十六进制预览窗口中选择“导出到文件”。"));
    overview->addStretch();

    auto *flow = addPage(tabs, QStringLiteral("数据流程"));
    flow->addWidget(textLabel(QStringLiteral("先加载结构定义。下面的生成、解析和导出都使用当前选中结构体的布局。"),
                              "helpBody", tabs));
    addFlow(flow, QStringLiteral("定义来源 → 当前布局"),
        {QStringLiteral("C/C++ 头文件\n或定义 JSON"), QStringLiteral("解析 / 校验\n选择结构体"),
         QStringLiteral("字段树与布局\n类型 · 大小 · 偏移")});
    addFlow(flow, QStringLiteral("生成数据 · 从字段值到十六进制"),
        {QStringLiteral("编辑字段值"), QStringLiteral("输入校验\n按布局小端编码"),
         QStringLiteral("十六进制预览\n复制 / 导出文本")});
    addFlow(flow, QStringLiteral("解析数据 · 从十六进制到字段值"),
        {QStringLiteral("粘贴十六进制\n选择字节序"), QStringLiteral("校验并解析\n预览字段结果"),
         QStringLiteral("点击填入\n更新可编辑字段")});
    addFlow(flow, QStringLiteral("直接导入 · 校验通过后更新字段"),
        {QStringLiteral("粘贴数据\n或读取 .hex / .txt"), QStringLiteral("按当前布局\n小端校验与解码"),
         QStringLiteral("更新字段值\n可继续编辑与生成")});
    addFlow(flow, QStringLiteral("复用定义 · 保存布局信息"),
        {QStringLiteral("当前结构体定义"), QStringLiteral("导出定义 JSON\n不含字段值"),
         QStringLiteral("再次打开 JSON\n还原字段与布局")});
    flow->addStretch();

    auto *notes = addPage(tabs, QStringLiteral("格式与限制"));
    addCard(notes, QStringLiteral("结构定义与数据文件的区别"),
        QStringLiteral("头文件或结构定义 JSON 描述“数据如何排列”；十六进制文本保存“这一次的数据”。"
                       "普通业务 JSON 不能作为结构定义加载。.hex / .txt 是十六进制文本，不是原始二进制文件。"));
    addCard(notes, QStringLiteral("字节序、长度与布局"),
        QStringLiteral("生成及直接导入使用小端序；解析预览可选大小端。布局使用 Windows x64 / MinGW 目标，"
                       "切换字节序不会重新计算偏移和对齐。输入字节数必须与结构体总大小完全一致，包含填充字节；数据上限为 1 MiB。"));
    addCard(notes, QStringLiteral("可编辑范围与往返限制"),
        QStringLiteral("支持基础整数、布尔、浮点、嵌套结构体及小端位域。数组暂不逐元素编辑，指针、引用、枚举及 const 等字段只读。"
                       "解析填回只更新可编辑值；重新生成时数组、填充字节和未命名位域补 0，只读字段和填充的原始字节不会保留。"
                       "NaN、无穷大等值可供解析查看，但不能填回编辑器。"));
    addCard(notes, QStringLiteral("遇到错误时"),
        QStringLiteral("红色输入框表示字段值无效，请根据提示修正后再生成数据。解析失败时检查十六进制格式、字节数和所选结构体。"
                       "联合体、继承、虚函数等不支持的布局，以及大端位域和大端 long double，会给出具体原因。"
                       "编辑值仅在本次运行中保留，关闭前请导出需要的数据。"));
    notes->addStretch();

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, dialog);
    buttons->button(QDialogButtonBox::Close)->setText(QStringLiteral("关闭"));
    QObject::connect(buttons, &QDialogButtonBox::rejected, dialog, &QDialog::close);
    layout->addWidget(buttons);
    dialog->show();
}
