
// FileBatcherDlg.cpp: 实现文件
//

#include "pch.h"
#include "framework.h"
#include "FileBatcher.h"
#include "FileBatcherDlg.h"
#include "afxdialogex.h"
#include"lua.hpp"
#include<fstream>
#include<vector>
#include<stack>
#include<ctime>
#define MAX_HELP_TEXT 500
#define MAX_TITLE_TEXT 10

#ifdef _DEBUG
#define new DEBUG_NEW
#endif
bool ifprogress = false;
CMutex lock_ifprogress;
struct lua_rule {
	CString name;//规则文件名
	CString title;//规则名
	CString help;//帮助信息
};//用于存储lua规则数据
struct file_move {
	CString from;
	CString to;
};//用于存储文件移动信息
std::vector < lua_rule>rulelist;//全局存储lua规则表
struct MF{
	CFileBatcherDlg* cfg;//窗口指针用于修改文本框
	std::stack<file_move>* movelist;//已存好文件信息的栈
	//各种参数
	bool ifcopy;//是否复制
	bool ifmes;//是否弹窗
	bool iflog;//是否记录日志
	//各种用来操纵的外部量
	int* failnum;//错误个数
	int* barnum;//完成个数
	CMutex* lock_barnum;//barnum的互斥锁
	CMutex* lock_stack;//栈的互斥锁
	CMutex* lock_text;//文本框的互斥锁
	CMutex* lock_log;//日志文件的互斥锁
	short num;//线程编号
	std::wofstream* log;//日志文件流
};//用于给文件移动线程传递参数
struct syncF {
	CFileBatcherDlg* cfg;//对话框指针
	int* barnum;//进度条位置
	CMutex* lock_barnum;//进度条互斥锁
	CMutex* lock_text;//文本框互斥锁
	bool* message;//进程间通信用的bool
};//用于给进度条同步函数传递参数
UINT SYNC(LPVOID lpParam)
{
	syncF* arg = (syncF*)lpParam;//传参数
	CString edt;
	CString ms;
	ms=_T("同步线程已创建");
	arg->lock_text->Lock();
	arg->cfg->GetDlgItemText(IDC_EDIT6, edt);
	edt.Append(ms);
	edt.Append(_T("\r\n"));
	arg->cfg->SetDlgItemText(IDC_EDIT6, edt.GetString());
	arg->lock_text->Unlock();//输出信息
	while (true)
	{
		Sleep(50);//每50ms进行一次同步
		arg->lock_barnum->Lock();
		arg->cfg->ProgressBar.SetPos(*(arg->barnum));
		if (arg->message)//如果收到信息就直接退出
		{
			arg->lock_barnum->Unlock();
			break;
		}
		arg->lock_barnum->Unlock();
	}
	ms = _T("同步线程已结束");
	arg->lock_text->Lock();
	arg->cfg->GetDlgItemText(IDC_EDIT6, edt);
	edt.Append(ms);
	edt.Append(_T("\r\n"));
	arg->cfg->SetDlgItemText(IDC_EDIT6, edt.GetString());
	arg->lock_text->Unlock();//输出信息
	return 0;
}
UINT movef(LPVOID lpParam)
{
	MF* arg = (MF*)lpParam;
	CString edt;
	CString ms;
	ms.Format(_T("线程%d已创建"), arg->num);//输出信息
	arg->lock_text->Lock();
	arg->cfg->GetDlgItemText(IDC_EDIT6, edt);
	edt.Append(ms);
	edt.Append(_T("\r\n"));
	arg->cfg->SetDlgItemText(IDC_EDIT6, edt.GetString());
	arg->lock_text->Unlock();
	ms.Empty();
	while (true)
	{
		
		arg->lock_stack->Lock();
		if (arg->movelist->empty())//如果待处理文件空了就退出
		{
			arg->lock_stack->Unlock();
			break;
		}
		file_move fm= arg->movelist->top();//取栈顶一个待处理项目
		arg->movelist->pop();
		arg->lock_stack->Unlock();
		bool wrong=false;
		arg->lock_barnum->Lock();//处理进度++
		(*(arg->barnum))++;
		arg->lock_barnum->Unlock();
		if (arg->ifcopy)//复制
		{
			std::fstream ifs;
			std::fstream ofs;
			ifs.open(fm.from.GetString(), std::ios::binary | std::ios::in);
			ofs.open(fm.to.GetString(), std::ios::binary | std::ios::out | std::ios::trunc);//内建复制功能以防万一
			if (!(ifs.is_open() && ofs.is_open()))//报错
			{
				wrong = true;
				CString ms;
				ms.Format(_T("来自线程%d，复制%s时错误，无法打开文件。"),arg->num, fm.from.GetString());
				if (arg->iflog)
				{
					arg->lock_log->Lock();
					*(arg->log) << ms.GetString() << std::endl;
					arg->lock_log->Unlock();
				}
				if(arg->ifmes)MessageBox(NULL, ms.GetString(), _T("错误"), MB_OK|MB_ICONERROR);
				CString edt;
				arg->lock_text->Lock();
				arg->cfg->GetDlgItemText(IDC_EDIT6, edt);
				edt.Append(ms);
				edt.Append(_T("\r\n"));
				arg->cfg->SetDlgItemText(IDC_EDIT6, edt);
				arg->lock_text->Unlock();
				ms.Empty();
				(*(arg->failnum))++;//失败数++
				continue;
			}
			ofs << ifs.rdbuf();//复制
			ifs.close();
			ofs.close();
		}
		else//移动
		{
			int i=_trename(fm.from.GetString(),fm.to.GetString());
			if (i==-1)//报错
			{
				wrong = true;
				CString ms;
				CString er;
				er.GetBufferSetLength(300);
				mbstowcs(er.GetBuffer(), strerror(errno), 300);
				er.ReleaseBuffer();
				ms.Format(_T("来自线程%d，移动%s时错误，错误描述：%s"),arg->num, fm.from.GetString(),er.GetString());
				if (arg->iflog)
				{
					arg->lock_log->Lock();
					*(arg->log) << ms.GetString() << std::endl;
					arg->lock_log->Unlock();
				}
				if(arg->ifmes)MessageBox(NULL, ms.GetString(), _T("错误"), MB_OK|MB_ICONERROR);
				CString edt;
				arg->lock_text->Lock();
				arg->cfg->GetDlgItemText(IDC_EDIT6, edt);
				edt.Append(ms);
				edt.Append(_T("\r\n"));
				arg->cfg->SetDlgItemText(IDC_EDIT6, edt);
				arg->lock_text->Unlock();
				ms.Empty();
				(*(arg->failnum))++;//失败数++
				continue;
			}
			if (arg->iflog&&(!wrong))//记录日志
			{
				if(arg->ifcopy)ms.Format(_T("来自线程%d，复制%s到%s成功"), arg->num, fm.from.GetString(), fm.to.GetString());
				else ms.Format(_T("来自线程%d，移动%s到%s成功"), arg->num, fm.from.GetString(), fm.to.GetString());
				arg->lock_log->Lock();
				*(arg->log) << ms.GetString() << std::endl;
				arg->lock_log->Unlock();
				ms.Empty();
			}
		}
		
	}	
		ms.Format(_T("线程%d已正常退出"),arg->num);//输出线程信息
		arg->lock_text->Lock();
		arg->cfg->GetDlgItemText(IDC_EDIT6, edt);
		edt.Append(ms);
		edt.Append(_T("\r\n"));
		arg->cfg->SetDlgItemText(IDC_EDIT6, edt.GetString());
		ms.Empty();
		arg->lock_text->Unlock();
		return 0;
}
UINT mainf(LPVOID lpParam)//控制线程用于控制文件处理线程，防止主线程因循环等待卡死
{
	lock_ifprogress.Lock();
	ifprogress = true;
	lock_ifprogress.Unlock();
	CFileBatcherDlg* cfg = (CFileBatcherDlg*)lpParam;
	CString edt;
	CString ms = _T("控制线程已创建");//输出信息
	cfg->GetDlgItemText(IDC_EDIT6, edt);
	edt.Append(ms);
	edt.Append(_T("\r\n"));
	cfg->SetDlgItemText(IDC_EDIT6, edt.GetString());
	clock_t c = clock();
	CString outdir;
	CString indir;
	cfg->GetDlgItemText(IDC_EDIT1, indir);//获取输入输出路径
	cfg->GetDlgItemText(IDC_EDIT2, outdir);
	if (indir.IsEmpty())//如果没有输入输出
	{
		cfg->MessageBox(_T("请输入输入路径"), _T("提示"), MB_OK);
		ms = _T("参数错误");
		cfg->GetDlgItemText(IDC_EDIT6, edt);
		edt.Append(ms);
		edt.Append(_T("\r\n"));
		cfg->SetDlgItemText(IDC_EDIT6, edt.GetString());
		return 1;
	}
	if (outdir.IsEmpty())
	{
		cfg->MessageBox(_T("请输入输出路径"), _T("提示"), MB_OK);
		ms = _T("参数错误");
		cfg->GetDlgItemText(IDC_EDIT6, edt);
		edt.Append(ms);
		edt.Append(_T("\r\n"));
		cfg->SetDlgItemText(IDC_EDIT6, edt.GetString());
		return 1;
	}
	if (indir[indir.GetLength() - 1] != '\\')indir.AppendChar('\\');//如果忘写最后的'\\'
	if (outdir[outdir.GetLength() - 1] != '\\')outdir.AppendChar('\\');
	if (_taccess(indir.GetString(), 0) == -1)//如果输入路径不存在或不可用
	{
		cfg->MessageBox(_T("输入目录无效"), _T("警告"), MB_OK | MB_ICONWARNING);
		ms = _T("参数错误");
		cfg->GetDlgItemText(IDC_EDIT6, edt);
		edt.Append(ms);
		edt.Append(_T("\r\n"));
		cfg->SetDlgItemText(IDC_EDIT6, edt.GetString());
		return 2;
	}
	int k=0;
	if (_taccess(outdir.GetString(), 0) == -1)k=_tmkdir(outdir.GetString());//如果输出目录不存在就创建一个
	if (k != 0)//如果创建也失败
	{
		cfg->MessageBox(_T("输出目录无效"), _T("警告"), MB_OK | MB_ICONWARNING);
		ms = _T("参数错误");
		cfg->GetDlgItemText(IDC_EDIT6, edt);
		edt.Append(ms);
		edt.Append(_T("\r\n"));
		cfg->SetDlgItemText(IDC_EDIT6, edt.GetString());
	}
	WIN32_FIND_DATA findfile;//寻找文件
	HANDLE fwnd = FindFirstFile((indir + _T("*.*")).GetString(), &findfile);//寻找第一个
	if (fwnd == INVALID_HANDLE_VALUE)//如果找不到
	{
		cfg->MessageBox(_T("输入目录读取错误"), _T("错误"), MB_OK | MB_ICONWARNING);
		ms = _T("输入目录读取错误");
		cfg->GetDlgItemText(IDC_EDIT6, edt);
		edt.Append(ms);
		edt.Append(_T("\r\n"));
		cfg->SetDlgItemText(IDC_EDIT6, edt.GetString());
		return 2;
	}
	std::stack<CString>files_in;//文件名栈用于存储文件名
	do//遍历寻找
	{
		CString buffer = findfile.cFileName;
		if (findfile.dwFileAttributes != FILE_ATTRIBUTE_DIRECTORY)
			files_in.push(buffer);
	} while (FindNextFile(fwnd, &findfile));
	CString buff;//获取每批文件个数
	cfg->GetDlgItemText(IDC_EDIT3, buff);
	int m = _ttoi(buff.GetString());
	int threadnum = cfg->TreadScroll.GetScrollPos();//获取线程数
	int n_max = 0;//包数
	switch (cfg->IsDlgButtonChecked(IDC_CHECK2))//计算包数
	{
	case BST_CHECKED:
		n_max = (files_in.size() / m) + 1;
		break;
	case BST_UNCHECKED:
		n_max = files_in.size() / m;
		break;
	}
	std::vector<CString>packs;//存储文件夹名
	for (int n = 1; n <= n_max; n++)
	{
		
		lua_State* L = luaL_newstate();//调用lua，初始化lua栈
		luaL_openlibs(L);
		CStringA luafile;
		luafile.GetBufferSetLength(MAX_PATH);
		wcstombs(luafile.GetBuffer(), rulelist[cfg->RuleCombo.GetCurSel()].name.GetString(), MAX_PATH);//lua不支持Unicodde这档事
		luafile.ReleaseBuffer();
		CStringA luapath = ".\\rules\\";
		luapath.Append(luafile);
		luaL_dofile(L, luapath.GetString());
		lua_getglobal(L, "settext");//获取全局函数settext
		lua_pushnumber(L, n);
		lua_pushnumber(L, m);
		lua_call(L, 2, 1);
		CString packname;
		packname.GetBufferSetLength(MAX_PATH);
		mbstowcs(packname.GetBuffer(), lua_tostring(L, -1), MAX_PATH);//获取返回值
		lua_close(L);
		packname.ReleaseBuffer();
		CString outdirnew(outdir.GetString());
		outdirnew.Append(packname.GetString());
		outdirnew.AppendChar('\\');
		if (_taccess(outdirnew.GetString(), 0) != -1)//同名文件夹问题
		{
			cfg->MessageBox(_T("输出目录下存在同名文件夹"), _T("错误"), MB_OK | MB_ICONERROR);
			ms = _T("输出目录下存在同名文件夹");
			cfg->GetDlgItemText(IDC_EDIT6, edt);
			edt.Append(ms);
			edt.Append(_T("\r\n"));
			cfg->SetDlgItemText(IDC_EDIT6, edt.GetString());
			return 3;
		}
		_tmkdir(outdirnew.GetString());//创建文件夹
		packs.push_back(outdirnew);
	}
	std::stack<file_move>movelist;//文件移动任务栈
	bool ifcopy = (((CButton*)cfg->GetDlgItem(IDC_CHECK1))->GetCheck()) == 1;//是否移动
	bool ifmes = (((CButton*)cfg->GetDlgItem(IDC_CHECK3))->GetCheck()) == 1;//是否报错
	int failnum = 0;//错误数
	CMutex lock_bar;//互斥锁
	CMutex lock_stack;
	CMutex lock_text;
	for (int i = 0; i < (n_max * m); i++)//初始化任务列表
	{
		file_move store;
		store.from = indir + files_in.top();
		store.to = packs[i / m] + files_in.top();
		movelist.push(store);
		files_in.pop();
	}
	std::wofstream log;//日志
	if (_taccess(_T(".\\logs\\"), 0) == -1)_tmkdir(_T("logs"));
	bool iflog = (((CButton*)cfg->GetDlgItem(IDC_CHECK4))->GetCheck()) == 1;//是否记录日志
	if (iflog)
	{
		CStringA logname;
		logname.GetBufferSetLength(260);
		time_t st;
		time(&st);
		tm tmd;
		localtime_s(&tmd, &st);
		strftime(logname.GetBuffer(), 260, ".\\logs\\%Y-%b-%d-%H:%M:%S-%a.log", &tmd);//日志名
		logname.ReleaseBuffer();
		log.open(logname.GetString(), std::ios::out | std::ios::trunc);
	}
	cfg->ProgressBar.SetRange(0, movelist.size());//初始化进度条
	cfg->ProgressBar.SetPos(0);
	int barnum=0;
	bool ext=false;
	syncF SF = { 0 };//进度条同步线程初始化
	SF.barnum = &barnum;
	SF.cfg = cfg;
	SF.lock_barnum = &lock_bar;
	SF.lock_text = &lock_text;
	SF.message = &ext;
	AfxBeginThread(SYNC, &SF);//创建线程
	std::vector<CWinThread*>threads;//线程指针列表
	for (short i = 0; i < threadnum; i++)
	{
		MF arg = { 0 };//文件处理线程参数
		arg.cfg = cfg;
		arg.movelist = &movelist;
		arg.ifcopy = ifcopy;
		arg.ifmes = ifmes;
		arg.failnum = &failnum;
		arg.lock_barnum = &lock_bar;
		arg.lock_stack = &lock_stack;
		arg.lock_text = &lock_text;
		arg.barnum = &barnum;
		arg.num = i;
		arg.iflog = iflog;
		arg.log = &log;
		CWinThread* st = AfxBeginThread(movef, &arg);//开始线程
		threads.push_back(st);//存储指针用于管理
	}
	for (int i = 0; i < threads.size();i++)
	{
		WaitForSingleObject(threads[i]->m_hThread, INFINITE);//等待所有文件处理线程退出
	}
	lock_bar.Lock();
	ext = true;//退出同步线程
	lock_bar.Unlock();
	cfg->ProgressBar.SetPos(barnum);//进度条拉满
	c = clock() - c;
	ms.Empty();
	ms.Format(_T("处理完成，共%d个,耗时 %d ms，错误 %d 个。"), n_max * m, c, failnum);//显示处理完成信息
	if (iflog)log << ms.GetString() << std::endl;//日志
	if (log.is_open())log.close();
	cfg->GetDlgItemText(IDC_EDIT6, edt);//输出信息
	edt.Append(ms);
	edt.Append(_T("\r\n"));
	cfg->SetDlgItemText(IDC_EDIT6, edt);
	cfg->MessageBox(ms.GetString(), _T("提示"), MB_OK);
	cfg->ProgressBar.SetPos(0);
	ms=_T("控制线程已退出");
	cfg->GetDlgItemText(IDC_EDIT6, edt);
	edt.Append(ms);
	edt.Append(_T("\r\n"));
	cfg->SetDlgItemText(IDC_EDIT6, edt.GetString());
	lock_ifprogress.Lock();
	ifprogress = false;
	lock_ifprogress.Unlock();
	return 0;
}
// CFileBatcherDlg 对话框



CFileBatcherDlg::CFileBatcherDlg(CWnd* pParent /*=nullptr*/)
	: CDialogEx(IDD_FILEBATCHER_DIALOG, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CFileBatcherDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_COMBO1, RuleCombo);
	//  DDX_Control(pDX, IDC_SCROLLBAR1, TreadScroll);
	DDX_Control(pDX, IDC_PROGRESS1, ProgressBar);
	DDX_Control(pDX, IDC_SCROLLBAR1, TreadScroll);
}

BEGIN_MESSAGE_MAP(CFileBatcherDlg, CDialogEx)
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN3, &CFileBatcherDlg::OnDeltaposSpin3)
	ON_BN_CLICKED(IDC_BUTTON1, &CFileBatcherDlg::OnBnClickedButton1)
	ON_BN_CLICKED(IDC_BUTTON2, &CFileBatcherDlg::OnBnClickedButton2)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN1, &CFileBatcherDlg::OnDeltaposSpin1)
	ON_NOTIFY(UDN_DELTAPOS, IDC_SPIN2, &CFileBatcherDlg::OnDeltaposSpin2)
	ON_EN_CHANGE(IDC_EDIT3, &CFileBatcherDlg::OnEnChangeEdit3)
	ON_EN_CHANGE(IDC_EDIT4, &CFileBatcherDlg::OnEnChangeEdit4)
	//	ON_NOTIFY(NM_THEMECHANGED, IDC_SCROLLBAR1, &CFileBatcherDlg::OnNMThemeChangedScrollbar1)
	ON_BN_CLICKED(IDC_BUTTON3, &CFileBatcherDlg::OnBnClickedButton3)
	ON_CBN_SELCHANGE(IDC_COMBO1, &CFileBatcherDlg::OnCbnSelchangeCombo1)
	ON_BN_CLICKED(IDCANCEL, &CFileBatcherDlg::OnBnClickedCancel)
	ON_EN_CHANGE(IDC_EDIT1, &CFileBatcherDlg::OnEnChangeEdit1)
	ON_EN_CHANGE(IDC_EDIT2, &CFileBatcherDlg::OnEnChangeEdit2)
	ON_WM_HSCROLL()
	ON_BN_CLICKED(IDOK, &CFileBatcherDlg::OnBnClickedOk)
//	ON_BN_CLICKED(IDC_CHECK1, &CFileBatcherDlg::OnBnClickedCheck1)
//	ON_BN_CLICKED(IDC_CHECK3, &CFileBatcherDlg::OnBnClickedCheck3)
	ON_BN_CLICKED(IDC_BUTTON4, &CFileBatcherDlg::OnBnClickedButton4)
	ON_BN_CLICKED(IDC_BUTTON5, &CFileBatcherDlg::OnBnClickedButton5)
	ON_EN_CHANGE(IDC_EDIT4, &CFileBatcherDlg::OnEnChangeEdit4)
	ON_WM_HSCROLL()
	ON_WM_CLOSE()
END_MESSAGE_MAP()


// CFileBatcherDlg 消息处理程序

BOOL CFileBatcherDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// 设置此对话框的图标。  当应用程序主窗口不是对话框时，框架将自动
	//  执行此操作
	SetIcon(m_hIcon, TRUE);			// 设置大图标
	SetIcon(m_hIcon, FALSE);		// 设置小图标

	// TODO: 在此添加额外的初始化代码
	if (_taccess(_T(".\\rules\\"), 0) == -1)_tmkdir(_T("rules"));//查看是否存在rules文件夹如果不存在则创建一个
	WIN32_FIND_DATA findrule;//搜索rules文件夹下的lua文件
	HANDLE hfind = FindFirstFile(_T(".\\rules\\*.lua"),&findrule);
	if (hfind == INVALID_HANDLE_VALUE)//失败则报错
	{
		MessageBox(_T("脚本不正确或不存在。"), _T("错误"), MB_OK|MB_ICONERROR);
		EndDialog(0);
		return TRUE;
	}
	
	do//载入lua脚本
	{
		
		if (findrule.dwFileAttributes == FILE_ATTRIBUTE_DIRECTORY)continue;//跳过目录
		CStringA a_na;//因为lua不支持Unicode所以要弄一些A字符串
		CStringA a_name;
		CString a_help;
		CString a_title;
		CString a_n = findrule.cFileName;
		a_na.GetBufferSetLength(MAX_PATH);
		wcstombs(a_na.GetBuffer(),a_n.GetString() , MAX_PATH);//获取文件名并转成multibyte
		a_na.ReleaseBuffer();
		a_name.Format(".\\rules\\%s", a_na.GetString());
		lua_State* L = luaL_newstate();//lua环境初始化
		luaL_openlibs(L);
		luaL_dofile(L, a_name.GetString());
		lua_getglobal(L, "helptext");//寻找帮助字符串
		if (!lua_isstring(L, -1))//没有则认定不是需要的规则
		{ 
			lua_close(L);
			continue;
		}
		a_help.GetBufferSetLength(MAX_HELP_TEXT);
		mbstowcs(a_help.GetBuffer(), lua_tostring(L, -1), MAX_HELP_TEXT);
		a_help.ReleaseBuffer();
		lua_getglobal(L, "title");//寻找标题
		if (!lua_isstring(L, -1))//同上
		{
			lua_close(L);
			continue;
		}
		a_title.GetBufferSetLength(MAX_TITLE_TEXT);
		mbstowcs(a_title.GetBuffer(), lua_tostring(L, -1), MAX_TITLE_TEXT);
		a_title.ReleaseBuffer();
		lua_getglobal(L, "settext");//寻找规则函数
		if(!lua_isfunction(L,-1))
		{
			lua_close(L);
			continue;
		}
		lua_pushnumber(L, 1);
		lua_pushnumber(L, 200);
		if(lua_pcall(L,2,1,0)!=0)//测试规则函数是否为传入两个数字返回一个字符串
		{
			lua_close(L);
			continue;
		}
		if (!lua_isstring(L, -1))
		{
			lua_close(L);
			continue;
		}
		lua_close(L);//关闭lua
		lua_rule store;
		store.name = a_n;
		store.title = a_title;
		store.help = a_help;
		rulelist.push_back(store);//存储规则
	} while (FindNextFile(hfind, &findrule));
	if(rulelist.empty())//没有有效规则
	{
		MessageBox(_T("脚本不正确或不存在。"), _T("错误"), MB_OK|MB_ICONERROR);
		EndDialog(0);
		return TRUE;
	}
	for (int i = 0; i < rulelist.size(); i++)
	{
		RuleCombo.InsertString(i,rulelist[i].title);//设置规则选项
	}
	RuleCombo.SetCurSel(0);
	SetDlgItemText(IDC_EDIT5, rulelist[0].help);
	SCROLLINFO tsinfo;
	tsinfo.fMask = SIF_RANGE|SIF_POS;
	tsinfo.nMin = 1;
	tsinfo.nMax = 16;
	tsinfo.nPos = 8;
	tsinfo.cbSize = sizeof(SCROLLINFO);
	TreadScroll.SetScrollInfo(&tsinfo);
	SetDlgItemText(IDC_EDIT4, _T("8"));
	SetDlgItemText(IDC_EDIT3, _T("600"));
	((CButton*)GetDlgItem(IDC_CHECK3))->SetCheck(1);
	return TRUE;  // 除非将焦点设置到控件，否则返回 TRUE
}

// 如果向对话框添加最小化按钮，则需要下面的代码
//  来绘制该图标。  对于使用文档/视图模型的 MFC 应用程序，
//  这将由框架自动完成。

void CFileBatcherDlg::OnPaint()
{
	if (IsIconic())
	{
		CPaintDC dc(this); // 用于绘制的设备上下文

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// 使图标在工作区矩形中居中
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// 绘制图标
		dc.DrawIcon(x, y, m_hIcon);
	}
	else
	{
		CDialogEx::OnPaint();
	}
}

//当用户拖动最小化窗口时系统调用此函数取得光标
//显示。
HCURSOR CFileBatcherDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}



void CFileBatcherDlg::OnDeltaposSpin3(NMHDR* pNMHDR, LRESULT* pResult)//+-1的spin
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	CString tpnum;
	GetDlgItemText(IDC_EDIT3, tpnum);
	int num = _ttoi(tpnum);
	switch (pNMUpDown->iDelta)
	{
	case 1:
		num = num - 1;
		break;
	case -1:
		num = num + 1;
	}
	tpnum.Format(_T("%d"), num);
	SetDlgItemText(IDC_EDIT3, tpnum);
	*pResult = 0;
}


void CFileBatcherDlg::OnBnClickedButton1()//目录选择按钮
{
	// TODO: 在此添加控件通知处理程序代码
	BROWSEINFO bi;
	ZeroMemory(&bi, sizeof(BROWSEINFO));
	TCHAR buf[MAX_PATH];
	
	
	bi.hwndOwner = NULL;//调用WindowsAPI
	bi.pidlRoot = NULL;
	bi.pszDisplayName = buf;
	bi.lpszTitle = _T("选择输入目录");
	bi.ulFlags = BIF_EDITBOX;
	bi.lpfn = NULL;
	bi.lParam = NULL;
	bi.iImage = IDR_MAINFRAME;
	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (pidl==NULL)return;
	CString buffer;
	buffer.GetBufferSetLength(MAX_PATH);
	SHGetPathFromIDList(pidl, buffer.GetBuffer());
	IMalloc* im = NULL;
	if (SUCCEEDED(SHGetMalloc(&im)))//释放内存防止内存泄漏
	{
		im->Free(pidl);
		im->Release();
	}
	buffer.ReleaseBuffer();
	CString estr1;
	CString estr2;
	GetDlgItemText(IDC_EDIT1, estr1);//设置字符串
	GetDlgItemText(IDC_EDIT2, estr2);
	SetDlgItemText(IDC_EDIT1, buffer.GetString());
	if (estr2.IsEmpty() || estr1 == estr2)
	SetDlgItemText(IDC_EDIT2, buffer.GetString());
}


void CFileBatcherDlg::OnBnClickedButton2()//类似上面的1
{
	// TODO: 在此添加控件通知处理程序代码
	BROWSEINFO bi;
	ZeroMemory(&bi, sizeof(BROWSEINFO));
	TCHAR buf[MAX_PATH];


	bi.hwndOwner = NULL;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = buf;
	bi.lpszTitle = _T("选择输出目录");
	bi.ulFlags = BIF_EDITBOX|BIF_NEWDIALOGSTYLE;
	bi.lpfn = NULL;
	bi.lParam = NULL;
	bi.iImage = IDR_MAINFRAME;
	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (pidl == NULL)return;
	CString buffer;
	buffer.GetBufferSetLength(MAX_PATH);
	SHGetPathFromIDList(pidl, buffer.GetBuffer());
	IMalloc* im = NULL;
	if (SUCCEEDED(SHGetMalloc(&im)))
	{
		im->Free(pidl);
		im->Release();
	}
	buffer.ReleaseBuffer();
	CString estr1;
	CString estr2;
	SetDlgItemText(IDC_EDIT2, buffer.GetString());
}


void CFileBatcherDlg::OnDeltaposSpin1(NMHDR* pNMHDR, LRESULT* pResult)//+-10的spin
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	CString tpnum;
	GetDlgItemText(IDC_EDIT3, tpnum);
	int num = _ttoi(tpnum);
	switch (pNMUpDown->iDelta)
	{
	case 1:
		num = num - 10;
		break;
	case -1:
		num = num + 10;
	}
	tpnum.Format(_T("%d"), num);
	SetDlgItemText(IDC_EDIT3, tpnum);
	*pResult = 0;
}


void CFileBatcherDlg::OnDeltaposSpin2(NMHDR* pNMHDR, LRESULT* pResult)//+-100的spin
{
	LPNMUPDOWN pNMUpDown = reinterpret_cast<LPNMUPDOWN>(pNMHDR);
	// TODO: 在此添加控件通知处理程序代码
	CString tpnum;
	GetDlgItemText(IDC_EDIT3, tpnum);
	int num = _ttoi(tpnum);
	switch (pNMUpDown->iDelta)
	{
	case 1:
		num = num - 100;
		break;
	case -1:
		num = num + 100;
	}
	tpnum.Format(_T("%d"), num);
	SetDlgItemText(IDC_EDIT3, tpnum);
	*pResult = 0;
}


void CFileBatcherDlg::OnEnChangeEdit3()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。
	CString tsnum;//限定输入在1~999999之间
	CString tsnum_s;
	int tnum;
	GetDlgItemText(IDC_EDIT3, tsnum);
	if (!tsnum.IsEmpty())
	{
		tnum = _ttoi(tsnum.GetString());
		if (tnum < 1)tnum = 1;
		if (tnum > 999999)tnum = 999999;
	}
	else
	{
		SetDlgItemText(IDC_EDIT3, _T("1"));
		return;
	}
	tsnum_s.Format(_T("%d"), tnum);
	if (tsnum != tsnum_s)SetDlgItemText(IDC_EDIT3, tsnum_s.GetString());
	// TODO:  在此添加控件通知处理程序代码
}


void CFileBatcherDlg::OnEnChangeEdit4()
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。
	CString tsnum;//限定输入在1~16之间
	CString tsnum_s;
	int tnum;
	GetDlgItemText(IDC_EDIT4, tsnum);
	if (!tsnum.IsEmpty())
	{
		tnum = _ttoi(tsnum.GetString());
		if (tnum < 1)tnum = 1;
		if (tnum > 16)tnum = 16;
	}
	else
	{ 
		SetDlgItemText(IDC_EDIT4, _T("1"));
		return;
	}
	tsnum_s.Format(_T("%d"), tnum);
	if (tsnum != tsnum_s)SetDlgItemText(IDC_EDIT4, tsnum_s.GetString());
	if (tnum != TreadScroll.GetScrollPos())TreadScroll.SetScrollPos(tnum);
	// TODO:  在此添加控件通知处理程序代码
}


void CFileBatcherDlg::OnBnClickedButton3()
{
	// TODO: 在此添加控件通知处理程序代码
	SetDlgItemText(IDC_EDIT3, _T("600"));	//默认600
}


void CFileBatcherDlg::OnCbnSelchangeCombo1()
{
	// TODO: 在此添加控件通知处理程序代码
	SetDlgItemText(IDC_EDIT5, rulelist[RuleCombo.GetCurSel()].help);//设置帮助文本
}


void CFileBatcherDlg::OnBnClickedCancel()
{
	// TODO: 在此添加控件通知处理程序代码
	CDialogEx::OnCancel();
}


void CFileBatcherDlg::OnEnChangeEdit1()//路径超过260字符时报错
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	CString buffer;
	GetDlgItemText(IDC_EDIT1, buffer);
	if (buffer.GetLength() > MAX_PATH)
	{
		SetDlgItemText(IDC_EDIT1, buffer.Left(MAX_PATH));
		MessageBox(_T("路径过长"), _T("警告"), MB_OK|MB_ICONWARNING);
	}
}


void CFileBatcherDlg::OnEnChangeEdit2()//类似上面
{
	// TODO:  如果该控件是 RICHEDIT 控件，它将不
	// 发送此通知，除非重写 CDialogEx::OnInitDialog()
	// 函数并调用 CRichEditCtrl().SetEventMask()，
	// 同时将 ENM_CHANGE 标志“或”运算到掩码中。

	// TODO:  在此添加控件通知处理程序代码
	CString buffer;
	GetDlgItemText(IDC_EDIT1, buffer);
	if (buffer.GetLength() > MAX_PATH)
	{
		SetDlgItemText(IDC_EDIT1, buffer.Left(MAX_PATH));
		MessageBox(_T("路径过长"), _T("警告"), MB_OK|MB_ICONWARNING);
	}
}


void CFileBatcherDlg::OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar)//滚动条控制
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	int ncPos = TreadScroll.GetScrollPos();
	switch (nSBCode)
	{
	case SB_LINELEFT:
		ncPos--;
		break;
	case SB_LINERIGHT:
		ncPos++;
		break;
	case SB_THUMBTRACK:
		ncPos = nPos;
		break;
	}
	if (ncPos > 16)ncPos = 16;
	if (ncPos < 1)ncPos = 1;
	if (ncPos == TreadScroll.GetScrollPos())return;
	TreadScroll.SetScrollPos(ncPos);
	CString num;
	num.Format(_T("%d"), ncPos);
	CString tsnum;
	GetDlgItemText(IDC_EDIT4, tsnum);
	if (num != tsnum)SetDlgItemText(IDC_EDIT4, num);
	CDialogEx::OnHScroll(nSBCode, nPos, pScrollBar);
}

void CFileBatcherDlg::OnBnClickedOk()//历史遗留问题
{

}

void CFileBatcherDlg::OnBnClickedButton4()//开始处理
{
	// TODO: 在此添加控件通知处理程序代码
	
	CFileBatcherDlg* cfg = (CFileBatcherDlg*)AfxGetApp()->GetMainWnd();//创建管理线程
	AfxBeginThread(mainf, cfg);
}


void CFileBatcherDlg::OnBnClickedButton5()//清空运行信息
{
	// TODO: 在此添加控件通知处理程序代码
	SetDlgItemText(IDC_EDIT6, _T(""));
}


void CFileBatcherDlg::OnClose()
{
	// TODO: 在此添加消息处理程序代码和/或调用默认值
	if (ifprogress)
	{
		if(IDOK==AfxMessageBox(_T("任务正在运行，确认关闭？"), MB_OKCANCEL | MB_ICONQUESTION))
			CDialogEx::OnClose();
	}
	else CDialogEx::OnClose();
}
