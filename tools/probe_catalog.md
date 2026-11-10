| class | part (id) | states probed (id) | tier | measurement set |
|---|---|---|---|---|
| Header | HP_HEADERITEM (1) | HIS_NORMAL(1) HIS_HOT(2) HIS_PRESSED(3) HIS_SORTEDNORMAL(4) HIS_SORTEDHOT(5) HIS_SORTEDPRESSED(6) HIS_ICONNORMAL(7) HIS_ICONHOT(8) HIS_ICONPRESSED(9) HIS_ICONSORTEDNORMAL(10) HIS_ICONSORTEDHOT(11) HIS_ICONSORTEDPRESSED(12) | P0/P1 full | all |
| Header | HP_HEADERITEMLEFT (2) | HILS_NORMAL(1) HILS_HOT(2) HILS_PRESSED(3) | P0/P1 full | all |
| Header | HP_HEADERITEMRIGHT (3) | HIRS_NORMAL(1) HIRS_HOT(2) HIRS_PRESSED(3) | P0/P1 full | all |
| Header | HP_HEADERSORTARROW (4) | HSAS_SORTEDUP(1) HSAS_SORTEDDOWN(2) | P0/P1 full | all |
| Header | HP_HEADERDROPDOWN (5) | HDDS_NORMAL(1) HDDS_SOFTHOT(2) HDDS_HOT(3) | P0/P1 full | all |
| Header | HP_HEADERDROPDOWNFILTER (6) | HDDFS_NORMAL(1) HDDFS_SOFTHOT(2) HDDFS_HOT(3) | P0/P1 full | all |
| Header | HP_HEADEROVERFLOW (7) | HOFS_NORMAL(1) HOFS_HOT(2) | P0/P1 full | all |
| Button | BP_PUSHBUTTON (1) | PBS_NORMAL(1) PBS_HOT(2) PBS_PRESSED(3) PBS_DISABLED(4) PBS_DEFAULTED(5) | P1 lean | sizes+margins+props |
| Button | BP_RADIOBUTTON (2) | RBS_UNCHECKEDNORMAL(1) RBS_UNCHECKEDHOT(2) RBS_CHECKEDNORMAL(5) RBS_CHECKEDDISABLED(8) | P1 lean | sizes+margins+props |
| Button | BP_CHECKBOX (3) | CBS_UNCHECKEDNORMAL(1) CBS_UNCHECKEDHOT(2) CBS_CHECKEDNORMAL(5) CBS_MIXEDNORMAL(9) | P1 lean | sizes+margins+props |
| Button | BP_GROUPBOX (4) | GBS_NORMAL(1) GBS_DISABLED(2) | P2 min | TS_TRUE+defined+content margins |
| Button | BP_USERBUTTON (5) | - | P2 min | TS_TRUE+defined+content margins |
| Button | BP_COMMANDLINK (6) | CMDLS_NORMAL(1) CMDLS_HOT(2) CMDLS_PRESSED(3) CMDLS_DISABLED(4) | P1 lean | sizes+margins+props |
| Button | BP_COMMANDLINKGLYPH (7) | CMDLGS_NORMAL(1) CMDLGS_HOT(2) CMDLGS_PRESSED(3) CMDLGS_DISABLED(4) | P1 lean | sizes+margins+props |
| Tab | TABP_TABITEM (1) | TIS_NORMAL(1) TIS_HOT(2) TIS_SELECTED(3) TIS_DISABLED(4) TIS_FOCUSED(5) | P1 lean | sizes+margins+props |
| Tab | TABP_TABITEMLEFTEDGE (2) | TILES_NORMAL(1) TILES_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_TABITEMRIGHTEDGE (3) | TIRES_NORMAL(1) TIRES_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_TABITEMBOTHEDGE (4) | TIBES_NORMAL(1) TIBES_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_TOPTABITEM (5) | TTIS_NORMAL(1) TTIS_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_TOPTABITEMLEFTEDGE (6) | TTILES_NORMAL(1) TTILES_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_TOPTABITEMRIGHTEDGE (7) | TTIRES_NORMAL(1) TTIRES_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_TOPTABITEMBOTHEDGE (8) | TTIBES_NORMAL(1) TTIBES_SELECTED(3) | P1 lean | sizes+margins+props |
| Tab | TABP_PANE (9) | - | P1 lean | sizes+margins+props |
| Tab | TABP_BODY (10) | - | P1 lean | sizes+margins+props |
| ListView | LVP_LISTITEM (1) | LISS_NORMAL(1) LISS_HOT(2) LISS_SELECTED(3) LISS_DISABLED(4) LISS_SELECTEDNOTFOCUS(5) LISS_HOTSELECTED(6) | P1 lean | sizes+margins+props |
| ListView | LVP_LISTGROUP (2) | - | P2 min | TS_TRUE+defined+content margins |
| ListView | LVP_LISTDETAIL (3) | - | P2 min | TS_TRUE+defined+content margins |
| ListView | LVP_LISTSORTEDDETAIL (4) | - | P2 min | TS_TRUE+defined+content margins |
| ListView | LVP_EMPTYTEXT (5) | - | P2 min | TS_TRUE+defined+content margins |
| ListView | LVP_GROUPHEADER (6) | - | P1 lean | sizes+margins+props |
| ListView | LVP_GROUPHEADERLINE (7) | - | P1 lean | sizes+margins+props |
| ListView | LVP_EXPANDBUTTON (8) | - | P2 min | TS_TRUE+defined+content margins |
| ListView | LVP_COLLAPSEBUTTON (9) | - | P1 lean | sizes+margins+props |
| ListView | LVP_COLUMNDETAIL (10) | - | P1 lean | sizes+margins+props |
| TreeView | TVP_TREEITEM (1) | TREIS_NORMAL(1) TREIS_HOT(2) TREIS_SELECTED(3) TREIS_DISABLED(4) TREIS_SELECTEDNOTFOCUS(5) TREIS_HOTSELECTED(6) | P1 lean | sizes+margins+props |
| TreeView | TVP_GLYPH (2) | GLPS_CLOSED(1) GLPS_OPENED(2) | P1 lean | sizes+margins+props |
| TreeView | TVP_BRANCH (3) | - | P1 lean | sizes+margins+props |
| TreeView | TVP_HOTGLYPH (4) | HGLPS_CLOSED(1) HGLPS_OPENED(2) | P1 lean | sizes+margins+props |
| Toolbar | TP_BUTTON (1) | TS_NORMAL(1) TS_HOT(2) TS_PRESSED(3) TS_DISABLED(4) TS_CHECKED(5) TS_HOTCHECKED(6) | P1 lean | sizes+margins+props |
| Toolbar | TP_DROPDOWNBUTTON (2) | TS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Toolbar | TP_SPLITBUTTON (3) | TS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Toolbar | TP_SPLITBUTTONDROPDOWN (4) | TS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Toolbar | TP_SEPARATOR (5) | TS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Toolbar | TP_SEPARATORVERT (6) | TS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Toolbar | TP_DROPDOWNBUTTONGLYPH (7) | TS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Rebar | RP_GRIPPER (1) | - | P1 lean | sizes+margins+props |
| Rebar | RP_GRIPPERVERT (2) | - | P1 lean | sizes+margins+props |
| Rebar | RP_BAND (3) | - | P1 lean | sizes+margins+props |
| Rebar | RP_CHEVRON (4) | CHEVS_NORMAL(1) CHEVS_HOT(2) CHEVS_PRESSED(3) | P2 min | TS_TRUE+defined+content margins |
| Rebar | RP_CHEVRONVERT (5) | CHEVSV_NORMAL(1) CHEVSV_HOT(2) CHEVSV_PRESSED(3) | P2 min | TS_TRUE+defined+content margins |
| Rebar | RP_BACKGROUND (6) | - | P1 lean | sizes+margins+props |
| Rebar | RP_SPLITTER (7) | - | P1 lean | sizes+margins+props |
| Rebar | RP_SPLITTERVERT (8) | - | P1 lean | sizes+margins+props |
| Progress | PP_BAR (1) | - | P1 lean | sizes+margins+props |
| Progress | PP_BARVERT (2) | - | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_CHUNK (3) | - | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_CHUNKVERT (4) | - | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_FILL (5) | PBFS_NORMAL(1) PBFS_ERROR(2) PBFS_PAUSED(3) PBFS_PARTIAL(4) | P1 lean | sizes+margins+props |
| Progress | PP_FILLVERT (6) | PBFVS_NORMAL(1) PBFVS_ERROR(2) PBFVS_PAUSED(3) PBFVS_PARTIAL(4) | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_PULSEOVERLAY (7) | - | P1 lean | sizes+margins+props |
| Progress | PP_MOVEOVERLAY (8) | - | P1 lean | sizes+margins+props |
| Progress | PP_PULSEOVERLAYVERT (9) | - | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_MOVEOVERLAYVERT (10) | - | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_TRANSPARENTBAR (11) | PBBS_NORMAL(1) PBBS_PARTIAL(2) | P2 min | TS_TRUE+defined+content margins |
| Progress | PP_TRANSPARENTBARVERT (12) | PBBVS_NORMAL(1) PBBVS_PARTIAL(2) | P2 min | TS_TRUE+defined+content margins |
| TrackBar | TKP_TRACK (1) | TRS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| TrackBar | TKP_TRACKVERT (2) | TRVS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| TrackBar | TKP_THUMB (3) | TUS_NORMAL(1) TUS_HOT(2) TUS_PRESSED(3) TUS_FOCUSED(4) TUS_DISABLED(5) | P1 lean | sizes+margins+props |
| TrackBar | TKP_THUMBBOTTOM (4) | TUBS_NORMAL(1) TUBS_HOT(2) | P1 lean | sizes+margins+props |
| TrackBar | TKP_THUMBTOP (5) | TUTS_NORMAL(1) TUTS_HOT(2) | P1 lean | sizes+margins+props |
| TrackBar | TKP_THUMBVERT (6) | TUVS_NORMAL(1) TUVS_HOT(2) | P1 lean | sizes+margins+props |
| TrackBar | TKP_THUMBLEFT (7) | TUVLS_NORMAL(1) TUVLS_HOT(2) | P1 lean | sizes+margins+props |
| TrackBar | TKP_THUMBRIGHT (8) | TUVRS_NORMAL(1) TUVRS_HOT(2) | P1 lean | sizes+margins+props |
| TrackBar | TKP_TICS (9) | TSS_NORMAL(1) | P1 lean | sizes+margins+props |
| TrackBar | TKP_TICSVERT (10) | TSVS_NORMAL(1) | P1 lean | sizes+margins+props |
| Spin | SPNP_UP (1) | UPS_NORMAL(1) UPS_HOT(2) UPS_PRESSED(3) UPS_DISABLED(4) | P1 lean | sizes+margins+props |
| Spin | SPNP_DOWN (2) | DNS_NORMAL(1) DNS_HOT(2) DNS_PRESSED(3) DNS_DISABLED(4) | P1 lean | sizes+margins+props |
| Spin | SPNP_UPHORZ (3) | UPHZS_NORMAL(1) UPHZS_HOT(2) | P1 lean | sizes+margins+props |
| Spin | SPNP_DOWNHORZ (4) | DNHZS_NORMAL(1) DNHZS_HOT(2) | P1 lean | sizes+margins+props |
| Edit | EP_EDITTEXT (1) | ETS_NORMAL(1) ETS_HOT(2) ETS_SELECTED(3) ETS_DISABLED(4) ETS_FOCUSED(5) ETS_READONLY(6) ETS_ASSIST(7) ETS_CUEBANNER(8) | P1 lean | sizes+margins+props |
| Edit | EP_CARET (2) | - | P2 min | TS_TRUE+defined+content margins |
| Edit | EP_BACKGROUND (3) | - | P1 lean | sizes+margins+props |
| Edit | EP_PASSWORD (4) | - | P1 lean | sizes+margins+props |
| Edit | EP_BACKGROUNDWITHBORDER (5) | EBWBS_NORMAL(1) EBWBS_HOT(2) EBWBS_DISABLED(3) EBWBS_FOCUSED(4) | P1 lean | sizes+margins+props |
| Edit | EP_EDITBORDER_NOSCROLL (6) | EPSN_NORMAL(1) EPSN_HOT(2) EPSN_FOCUSED(3) EPSN_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Edit | EP_EDITBORDER_HSCROLL (7) | EPSH_NORMAL(1) EPSH_HOT(2) EPSH_FOCUSED(3) EPSH_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Edit | EP_EDITBORDER_VSCROLL (8) | EPSV_NORMAL(1) EPSV_HOT(2) EPSV_FOCUSED(3) EPSV_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Edit | EP_EDITBORDER_HVSCROLL (9) | EPSHV_NORMAL(1) EPSHV_HOT(2) EPSHV_FOCUSED(3) EPSHV_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_DROPDOWNBUTTON (1) | CBXS_NORMAL(1) CBXS_HOT(2) CBXS_PRESSED(3) CBXS_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_BACKGROUND (2) | - | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_TRANSPARENTBACKGROUND (3) | - | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_BORDER (4) | CBB_NORMAL(1) CBB_HOT(2) CBB_FOCUSED(3) CBB_DISABLED(4) | P1 lean | sizes+margins+props |
| Combobox | CP_READONLY (5) | CBRO_NORMAL(1) CBRO_HOT(2) CBRO_PRESSED(3) CBRO_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_DROPDOWNBUTTONRIGHT (6) | CBXSR_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_DROPDOWNBUTTONLEFT (7) | CBXSL_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Combobox | CP_CUEBANNER (8) | CBCB_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Scrollbar | SBP_ARROWBTN (1) | ABS_UPNORMAL(1) ABS_UPHOT(2) ABS_UPPRESSED(3) ABS_UPDISABLED(4) ABS_DOWNNORMAL(5) ABS_LEFTNORMAL(9) ABS_RIGHTNORMAL(13) | P1 lean | sizes+margins+props |
| Scrollbar | SBP_THUMBBTNHORZ (2) | SCRBS_NORMAL(1) SCRBS_HOT(2) SCRBS_PRESSED(3) SCRBS_DISABLED(4) | P1 lean | sizes+margins+props |
| Scrollbar | SBP_THUMBBTNVERT (3) | SCRBS_NORMAL(1) SCRBS_HOT(2) SCRBS_PRESSED(3) SCRBS_DISABLED(4) | P1 lean | sizes+margins+props |
| Scrollbar | SBP_LOWERTRACKHORZ (4) | SCRBS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Scrollbar | SBP_UPPERTRACKHORZ (5) | SCRBS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Scrollbar | SBP_LOWERTRACKVERT (6) | SCRBS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Scrollbar | SBP_UPPERTRACKVERT (7) | SCRBS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Scrollbar | SBP_GRIPPERHORZ (8) | SCRBS_NORMAL(1) | P1 lean | sizes+margins+props |
| Scrollbar | SBP_GRIPPERVERT (9) | SCRBS_NORMAL(1) | P1 lean | sizes+margins+props |
| Scrollbar | SBP_SIZEBOX (10) | SZB_RIGHTALIGN(1) SZB_LEFTALIGN(2) | P1 lean | sizes+margins+props |
| Status | SP_PANE (1) | - | P1 lean | sizes+margins+props |
| Status | SP_GRIPPERPANE (2) | - | P1 lean | sizes+margins+props |
| Status | SP_GRIPPER (3) | - | P1 lean | sizes+margins+props |
| Tooltip | TTP_STANDARD (1) | TTSS_NORMAL(1) TTSS_LINK(2) | P1 lean | sizes+margins+props |
| Tooltip | TTP_STANDARDTITLE (2) | TTSS_NORMAL(1) TTSS_LINK(2) | P2 min | TS_TRUE+defined+content margins |
| Tooltip | TTP_BALLOON (3) | TTBS_NORMAL(1) TTBS_LINK(2) | P1 lean | sizes+margins+props |
| Tooltip | TTP_BALLOONTITLE (4) | TTBS_NORMAL(1) TTBS_LINK(2) | P2 min | TS_TRUE+defined+content margins |
| Tooltip | TTP_CLOSE (5) | TTCS_NORMAL(1) TTCS_HOT(2) TTCS_PRESSED(3) | P1 lean | sizes+margins+props |
| Tooltip | TTP_BALLOONSTEM (6) | TTBSS_POINTINGUPLEFTWALL(1) TTBSS_POINTINGUPCENTERED(2) TTBSS_POINTINGUPRIGHTWALL(3) | P1 lean | sizes+margins+props |
| Tooltip | TTP_WRENCH (7) | TTWS_NORMAL(1) TTWS_HOT(2) TTWS_PRESSED(3) | P1 lean | sizes+margins+props |
| Window | WP_CAPTION (1) | CS_ACTIVE(1) CS_INACTIVE(2) CS_DISABLED(3) | P1 lean | sizes+margins+props |
| Window | WP_SMALLCAPTION (2) | CS_ACTIVE(1) CS_INACTIVE(2) | P1 lean | sizes+margins+props |
| Window | WP_MINCAPTION (3) | MXCS_ACTIVE(1) | P1 lean | sizes+margins+props |
| Window | WP_MAXCAPTION (5) | MXCS_ACTIVE(1) | P1 lean | sizes+margins+props |
| Window | WP_FRAMELEFT (7) | FS_ACTIVE(1) FS_INACTIVE(2) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_FRAMEBOTTOM (9) | FS_ACTIVE(1) FS_INACTIVE(2) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_SYSBUTTON (13) | SBS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_MINBUTTON (15) | MINBS_NORMAL(1) MINBS_HOT(2) MINBS_PUSHED(3) MINBS_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_MAXBUTTON (17) | MAXBS_NORMAL(1) MAXBS_HOT(2) MAXBS_PUSHED(3) MAXBS_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_CLOSEBUTTON (18) | CBS_NORMAL(1) CBS_HOT(2) CBS_PUSHED(3) CBS_DISABLED(4) | P1 lean | sizes+margins+props |
| Window | WP_SMALLCLOSEBUTTON (19) | CBS_NORMAL(1) CBS_HOT(2) CBS_PUSHED(3) CBS_DISABLED(4) | P1 lean | sizes+margins+props |
| Window | WP_RESTOREBUTTON (21) | RBS_NORMAL(1) RBS_HOT(2) RBS_PUSHED(3) RBS_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_HELPBUTTON (23) | HBS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_HORZSCROLL (25) | HSS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_VERTSCROLL (27) | VSS_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Window | WP_DIALOG (29) | - | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_BARBACKGROUND (7) | MB_ACTIVE(1) MB_INACTIVE(2) | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_BARITEM (8) | MBI_NORMAL(1) MBI_HOT(2) MBI_PUSHED(3) MBI_DISABLED(4) | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPBACKGROUND (9) | - | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPBORDERS (10) | - | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPCHECK (11) | MC_CHECKMARKNORMAL(1) MC_CHECKMARKDISABLED(2) MC_BULLETNORMAL(3) | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPCHECKBACKGROUND (12) | MCB_DISABLED(1) MCB_NORMAL(2) MCB_BITMAP(3) | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPGUTTER (13) | - | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPITEM (14) | MPI_NORMAL(1) MPI_HOT(2) MPI_DISABLED(3) MPI_DISABLEDHOT(4) | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPSEPARATOR (15) | - | P2 min | TS_TRUE+defined+content margins |
| Menu | MENU_POPUPSUBMENU (16) | MSM_NORMAL(1) MSM_DISABLED(2) | P2 min | TS_TRUE+defined+content margins |
| Listbox | LBCP_BORDER_HSCROLL (1) | LBPSH_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Listbox | LBCP_BORDER_HVSCROLL (2) | LBPSHV_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Listbox | LBCP_BORDER_NOSCROLL (3) | LBPSN_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Listbox | LBCP_BORDER_VSCROLL (4) | LBPSV_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| Listbox | LBCP_ITEM (5) | LBPSI_HOT(1) | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_BACKGROUND (1) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_BORDERS (2) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_GRIDBACKGROUND (3) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_COLHEADERSPLITTER (4) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_GRIDCELLBACKGROUND (5) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_GRIDCELL (6) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_GRIDCELLUPPER (7) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_TRAILINGGRIDCELL (8) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_TRAILINGGRIDCELLUPPER (9) | - | P2 min | TS_TRUE+defined+content margins |
| MonthCal | MC_NAVNEXT (10) | MCNN_NORMAL(1) | P1 lean | sizes+margins+props |
| MonthCal | MC_NAVPREV (11) | MCNP_NORMAL(1) | P2 min | TS_TRUE+defined+content margins |
| DatePicker | DP_DATETEXT (1) | DPDT_NORMAL(1) | P1 lean | sizes+margins+props |
| DatePicker | DP_DATEBORDER (2) | DPDB_NORMAL(1) | P1 lean | sizes+margins+props |
| DatePicker | DP_SHOWCALENDARBUTTONRIGHT (3) | DPSCBR_NORMAL(1) | P1 lean | sizes+margins+props |
| Static | (class level) (0) | - | P1 lean | sizes+margins+props |
| Link | LP_HYPERLINK (1) | HLS_NORMALTEXT(1) HLS_LINKTEXT(2) | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_MAININSTRUCTION (1) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_INSTRUCTION (2) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_BODYTITLE (3) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_BODYTEXT (4) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_SECONDARYTEXT (5) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_HYPERLINKTEXT (6) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_EXPANDED (7) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_LABEL (8) | - | P2 min | TS_TRUE+defined+content margins |
| TextStyle | TEXT_CONTROLLABEL (9) | - | P2 min | TS_TRUE+defined+content margins |
| Globals | (class level) (0) | - | P2 min | TS_TRUE+defined+content margins |
