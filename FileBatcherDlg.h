
// FileBatcherDlg.h: 头文件
//

#pragma once


// CFileBatcherDlg 对话框
class CFileBatcherDlg : public CDialogEx
{
// 构造
public:
	CFileBatcherDlg(CWnd* pParent = nullptr);	// 标准构造函数

// 对话框数据
#ifdef AFX_DESIGN_TIME
	enum { IDD = IDD_FILEBATCHER_DIALOG };
#endif

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);	// DDX/DDV 支持


// 实现
protected:
	HICON m_hIcon;

	// 生成的消息映射函数
	virtual BOOL OnInitDialog();
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()
public:
	afx_msg void OnDeltaposSpin3(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBnClickedButton1();
	afx_msg void OnBnClickedButton2();
	afx_msg void OnDeltaposSpin1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnDeltaposSpin2(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnEnChangeEdit3();
	//afx_msg void OnEnChangeEdit4();
//	afx_msg void OnNMThemeChangedScrollbar1(NMHDR* pNMHDR, LRESULT* pResult);
	afx_msg void OnBnClickedButton3();
	afx_msg void OnCbnSelchangeCombo1();
	afx_msg void OnBnClickedCancel();
	CComboBox RuleCombo;
//	CScrollBar TreadScroll;
	CProgressCtrl ProgressBar;
	afx_msg void OnEnChangeEdit1();
	afx_msg void OnEnChangeEdit2();
	//afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnBnClickedOk();
//	afx_msg void OnBnClickedCheck1();
//	afx_msg void OnBnClickedCheck3();
	afx_msg void OnBnClickedButton4();
	afx_msg void OnBnClickedButton5();
	afx_msg void OnEnChangeEdit4();
	CScrollBar TreadScroll;
	afx_msg void OnHScroll(UINT nSBCode, UINT nPos, CScrollBar* pScrollBar);
	afx_msg void OnClose();
};
