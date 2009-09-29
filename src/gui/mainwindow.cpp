/***************************************************************************
                          mainwindow.cpp  -  main window
                             -------------------
    begin                : 10 Oct 2005
    copyright            : (C) 2005-2007 Michal Rudolf <mrudolf@kdewebdev.org>
 ***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "boardsetup.h"
#include "boardview.h"
#include "copydialog.h"
#include "chessbrowser.h"
#include "databaseinfo.h"
#include "ecothread.h"
#include "filtermodel.h"
#include "game.h"
#include "gamelist.h"
#include "helpwindow.h"
#include "mainwindow.h"
#include "messagedialog.h"
#include "memorydatabase.h"
#include "openingtree.h"
#include "output.h"
#include "pgndatabase.h"
#include "playerdialog.h"
#include "preferences.h"
#include "savedialog.h"
#include "settings.h"
#include "tablebase.h"
#include "tableview.h"
#include "tipoftheday.h"
#include "analysiswidget.h"
#include "version.h"

#include <time.h>

#include <QtGui>

MainWindow::MainWindow() : QMainWindow(),
		m_playerDialog(0), m_saveDialog(0), m_helpWindow(0), m_tipDialog(0),
		m_showPgnSource(false)
{
	setObjectName("MainWindow");

	/* Create clipboard database */
	m_databases.append(new DatabaseInfo);
	m_currentDatabase = 0;
	
	m_tablebase = new Shredder;
	connect(m_tablebase, SIGNAL(bestMove(Move, int)), this, SLOT(showTablebaseMove(Move, int)));

	/* Actions */
	m_actions = new QActionGroup(this);
	m_actions->setExclusive(false);
	setupActions();

	/* Delete on close */
	setAttribute(Qt::WA_DeleteOnClose);

	/* Recent files */
	m_recentFiles.restore("History", "RecentFiles");
	m_recentFiles.removeMissingFiles();
	updateMenuRecent();

	/* Output */
	m_output = new Output(Output::NotationWidget);

	setDockNestingEnabled(true);

	/* Board */
	m_boardSplitter = new QSplitter(Qt::Vertical);
	m_boardSplitter->setChildrenCollapsible(false);
	setCentralWidget(m_boardSplitter);
	m_boardView = new BoardView(m_boardSplitter);
	m_boardView->setObjectName("BoardView");
	m_boardView->setMinimumSize(200, 200);
	m_boardView->resize(500, 540);
	connect(this, SIGNAL(reconfigure()), m_boardView, SLOT(configure()));
	connect(m_boardView, SIGNAL(moveMade(Square, Square)), SLOT(slotBoardMove(Square, Square)));
	connect(m_boardView, SIGNAL(clicked(Square, int)), SLOT(slotBoardClick(Square, int)));
	connect(m_boardView, SIGNAL(wheelScrolled(int)), SLOT(slotBoardMoveWheel(int)));

	/* Move view */
	m_moveView = new ChessBrowser(m_boardSplitter);
	m_moveView->setMinimumHeight(80);
	m_moveView->slotReconfigure();
	connect(m_moveView, SIGNAL(anchorClicked(const QUrl&)), SLOT(slotGameViewLink(const QUrl&)));

	/* Board layout */
	m_boardSplitter->addWidget(m_boardView);
	m_boardSplitter->addWidget(m_moveView);

	/* Game view */
	QDockWidget* dock = new QDockWidget(tr("Game Text"), this);
	dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	dock->setObjectName("GameTextDock");
	m_gameView = new ChessBrowser(dock, true);
	m_gameView->setMinimumSize(150, 100);
	m_gameView->slotReconfigure();
	connect(m_gameView, SIGNAL(anchorClicked(const QUrl&)), SLOT(slotGameViewLink(const QUrl&)));
	connect(m_gameView, SIGNAL(actionRequested(int, int)), SLOT(slotGameModify(int, int)));
	connect(this, SIGNAL(databaseChanged(DatabaseInfo*)), m_gameView, SLOT(slotDatabaseChanged(DatabaseInfo*)));
	dock->setWidget(m_gameView);
	addDockWidget(Qt::RightDockWidgetArea, dock);
	m_menuView->addAction(dock->toggleViewAction());
	dock->toggleViewAction()->setShortcut(Qt::CTRL + Qt::Key_P);

	/* Game List */
	dock = new QDockWidget(tr("Game List"), this);
	dock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
	dock->setObjectName("GameList");
	m_gameList = new GameList(databaseInfo()->filter(), dock);
	m_gameList->setMinimumSize(150, 100);
	connect(m_gameList, SIGNAL(selected(int)), SLOT(slotFilterLoad(int)));
	connect(m_gameList, SIGNAL(searchDone()), SLOT(slotFilterChanged()));
	dock->setWidget(m_gameList);
	addDockWidget(Qt::BottomDockWidgetArea, dock);
	m_menuView->addAction(dock->toggleViewAction());
	dock->toggleViewAction()->setShortcut(Qt::CTRL + Qt::Key_L);

	/* Opening Tree */
	dock = new QDockWidget(tr("Opening Tree"), this);
	dock->setObjectName("OpeningTreeDock");
	m_openingTree = new TableView(dock);
	m_openingTree->setObjectName("OpeningTree");
	m_openingTree->setMinimumSize(150, 100);
	m_openingTree->setSortingEnabled(true);
	m_openingTree->setModel(new OpeningTree);
	m_openingTree->sortByColumn(1, Qt::DescendingOrder);
	m_openingTree->slotReconfigure();
	connect(m_openingTree, SIGNAL(clicked(const QModelIndex&)),
		SLOT(slotSearchTreeMove(const QModelIndex&)));
	dock->setWidget(m_openingTree);
	addDockWidget(Qt::RightDockWidgetArea, dock);
	m_menuView->addAction(dock->toggleViewAction());
	connect(dock->toggleViewAction(), SIGNAL(triggered()), SLOT(slotSearchTree()));
	dock->toggleViewAction()->setShortcut(Qt::CTRL + Qt::Key_T);

	/* Analysis Dock */
	dock = new QDockWidget(tr("Analysis"), this);
	dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	dock->setObjectName("AnalysisDock");
	m_analysis = new AnalysisWidget();
	m_analysis->setMinimumSize(150, 100);
	dock->setWidget(m_analysis);
	addDockWidget(Qt::RightDockWidgetArea, dock);
	QAction *action = dock->toggleViewAction();
	m_menuView->addAction(action);
	connect(this, SIGNAL(boardChange(const Board&)), m_analysis, SLOT(setPosition(const Board&)));
	connect(this, SIGNAL(reconfigure()), m_analysis, SLOT(slotReconfigure()));
	// Make sure engine is disabled if dock is hidden
	connect(action, SIGNAL(toggled(bool)), m_analysis, SLOT(setShown(bool)));
	dock->toggleViewAction()->setShortcut(Qt::CTRL + Qt::SHIFT + Qt::Key_A);

	/* Randomize */
	srand(time(0));

	/* Restoring layouts */
	if (!AppSettings->layout(this))
		resize(800, 600);
	AppSettings->beginGroup("MainWindow");
	m_boardSplitter->restoreState(AppSettings->value("BoardSplit").toByteArray());
	AppSettings->endGroup();
	slotReconfigure();

	/* Status */
	m_statusFilter = new QLabel(statusBar());
	statusBar()->addPermanentWidget(m_statusFilter);
	m_progressBar = new QProgressBar;

	/* Reset board - not earlier, as all widgets have to be created. */
	slotGameChanged();

	/* Display main window */
	show();

	/* Load files from command line */
	QStringList args = qApp->arguments();
	for (int i = 1; i < args.count(); i++)
		if (QFile::exists(args[i]))
			openDatabase(args[i]);

	/* Activate clipboard */
	updateMenuDatabases();
	slotDatabaseChanged();

	/* Tip of the day */
	AppSettings->beginGroup("/Tips/");
	if (AppSettings->value("showTips", true).toBool())
		slotHelpTip();
	AppSettings->endGroup();

	/* Load ECO file */
	slotStatusMessage(tr("Loading ECO file..."));
	qApp->setOverrideCursor(Qt::WaitCursor);
	m_ecothread = new EcoThread(AppSettings->dataPath() + "/chessx");
	connect(m_ecothread, SIGNAL(loaded()), this, SLOT(ecoLoaded()));
	m_ecothread->start();

	m_timer = new QTimer(this);
	m_timer->setInterval(100);
	m_timer->setSingleShot(true);
   connect(m_timer, SIGNAL(timeout()), this, SLOT(slotGameLoadPending()));

}

MainWindow::~MainWindow()
{
	m_timer->stop();
	/* Stop analysis. */
	m_analysis->analyze(false);
	qDeleteAll(m_databases.begin(), m_databases.end());
	delete m_saveDialog;
	delete m_playerDialog;
	delete m_helpWindow;
	delete m_output;
	delete m_tipDialog;
	delete m_tablebase;
}

void MainWindow::ecoLoaded()
{
	qApp->restoreOverrideCursor();
	slotStatusMessage(tr("ECO Loaded."));
	m_ecothread->wait();
	delete m_ecothread;
	m_ecothread = NULL;
}

void MainWindow::closeEvent(QCloseEvent* e)
{
	if (confirmQuit()) {
		m_recentFiles.save("History", "RecentFiles");
		AppSettings->setLayout(m_playerDialog);
		AppSettings->setLayout(m_helpWindow);
		m_gameList->saveConfig();
		m_openingTree->saveConfig();
		m_gameView->saveConfig();
		m_moveView->saveConfig();
		AppSettings->setLayout(this);
		AppSettings->beginGroup("MainWindow");
		AppSettings->setValue("BoardSplit", m_boardSplitter->saveState());
		AppSettings->endGroup();
		e->accept();
		qApp->quit();
	} else
		e->ignore();
}

DatabaseInfo* MainWindow::databaseInfo()
{
	return m_databases[m_currentDatabase];
}

const DatabaseInfo* MainWindow::databaseInfo() const
{
	return m_databases[m_currentDatabase];
}

Database* MainWindow::database()
{
	return databaseInfo()->database();
}

QString MainWindow::databaseName(int index) const
{
	if (index < 0) index = m_currentDatabase;
	QString name = m_databases[index]->database()->name();
	if (name.isEmpty())
		return tr("[Clipboard]");
	return name;
}

Game& MainWindow::game()
{
	return databaseInfo()->currentGame();
}

int MainWindow::gameIndex() const
{
	return databaseInfo()->currentIndex();
}

void MainWindow::gameLoad(int index, bool force, bool reload)
{
	if (databaseInfo()->loadGame(index, reload))
		m_gameList->selectGame(index);
	else if (!force)
		return;
	else
		databaseInfo()->newGame();
	slotGameChanged();
}

void MainWindow::gameMoveBy(int change)
{
	if (game().moveByPly(change)) {
		slotMoveChanged();
		m_gameView->setFocus();
	}
}

void MainWindow::updateMenuRecent()
{
	for (int i = 0; i < m_recentFiles.count(); i++) {
		m_recentFileActions[i]->setVisible(true);
		m_recentFileActions[i]->setText(QString("&%1: %2").arg(i + 1).arg(m_recentFiles[i]));
		m_recentFileActions[i]->setData(m_recentFiles[i]);
	}
	for (int i = m_recentFiles.count(); i < m_recentFileActions.count(); i++)
		m_recentFileActions[i]->setVisible(false);
}

void MainWindow::updateMenuDatabases()
{
	while (m_databases.count() > m_databaseActions.count()) {
		QAction* action = new QAction(this);
		connect(action, SIGNAL(triggered()), SLOT(slotDatabaseChange()));
		m_databaseActions.append(action);
		m_menuDatabases->addAction(action);
	}
	for (int i = 0; i < m_databases.count(); i++) {
		m_databaseActions[i]->setVisible(true);
		m_databaseActions[i]->setData(i);
		m_databaseActions[i]->setText(QString("&%1: %2").arg(i).arg(databaseName(i)));
		int key = Qt::CTRL + Qt::Key_1 + (i - 1);
		if (i < 10)
			m_databaseActions[i]->setShortcut(key);
	}
	for (int i = m_databases.count(); i < m_databaseActions.count(); i++) {
		m_databaseActions[i]->setVisible(false);
		m_databaseActions[i]->setShortcut(0);
	}
}

bool MainWindow::openDatabase(const QString& fname)
{
	/* Check if the database isn't already open */
	for (int i = 0; i < m_databases.count(); i++)
		if (m_databases[i]->database()->filename() == fname) {
			m_currentDatabase = i;
			slotDatabaseChanged();
			slotStatusMessage(tr("Database %1 is already opened.").arg(fname.section('/', -1)));
			return false;
		}

	QTime time;
	time.start();
	// Create database, connect progress bar and open file
	DatabaseInfo* db = new DatabaseInfo(fname);
	connect(db->database(), SIGNAL(fileOpened(const QString&)), SLOT(slotStatusFileOpened(const QString&)));
	connect(db->database(), SIGNAL(fileClosed(const QString&)), SLOT(slotStatusFileClosed(const QString&)));
	connect(db->database(), SIGNAL(fileProgress(int)), SLOT(slotStatusProgress(int)));

	if (!db->open()) {
		delete db;
		return false;
	}

	m_databases.append(db);
	m_currentDatabase = m_databases.count() - 1;
	m_recentFiles.append(fname);
	
	updateMenuRecent();
	updateMenuDatabases();
	slotStatusMessage(tr("Database %1 opened successfully (%2 seconds).")
			  .arg(fname.section('/', -1)).arg((time.elapsed() / 100 / 10.0)));
	slotDatabaseChanged();
	return true;
}

QString MainWindow::exportFileName(int& format)
{
	QFileDialog fd(this);
	fd.setAcceptMode(QFileDialog::AcceptSave);
	fd.setFileMode(QFileDialog::AnyFile);
	fd.setWindowTitle(tr("Export games"));
	fd.setViewMode(QFileDialog::Detail);
	fd.setDirectory(QDir::homePath());
	QStringList filters;
	filters << tr("PGN file (*.pgn)")
	<< tr("HTML page (*.html)")
	<< tr("LaTeX document (*.tex)");
	fd.setFilters(filters);
	if (fd.exec() != QDialog::Accepted)
		return QString();
	if (fd.selectedFilter().contains("*.tex"))
		format = Output::Latex;
	else if (fd.selectedFilter().contains("*.html"))
		format = Output::Html;
	else format = Output::Pgn;
	return fd.selectedFiles().first();
}

bool MainWindow::gameEditComment(Output::CommentType type)
{
	bool ok;
	QString annotation;
	if (type == Output::Precomment)
		annotation = game().annotation(CURRENT_MOVE, Game::BeforeMove);
	else annotation = game().annotation();
	QString cmt = QInputDialog::getText(this, tr("Edit comment"), tr("Comment:"),
					    QLineEdit::Normal, annotation, &ok);
	if (ok) {
		if (type == Output::Precomment)
			game().setAnnotation(cmt, CURRENT_MOVE, Game::BeforeMove);
		else game().setAnnotation(cmt);
	}
	return ok;
}

PlayerDialog* MainWindow::playerDialog()
{
	if (!m_playerDialog) {
		m_playerDialog = new PlayerDialog(this);
		AppSettings->layout(m_playerDialog);
	}
	return m_playerDialog;
}

SaveDialog* MainWindow::saveDialog()
{
	if (!m_saveDialog)
		m_saveDialog = new SaveDialog(this);
	return m_saveDialog;
}

HelpWindow* MainWindow::helpWindow()
{
	if (!m_helpWindow) {
		m_helpWindow = new HelpWindow;
		AppSettings->layout(m_helpWindow);
	}
	return m_helpWindow;
}

TipOfDayDialog* MainWindow::tipDialog()
{
	if (!m_tipDialog)
		m_tipDialog = new TipOfDayDialog(this);
	return m_tipDialog;
}

void MainWindow::slotFileNew()
{
	QString file = QFileDialog::getSaveFileName(this, tr("New database"),
			AppSettings->value("/General/databasePath").toString(),
			tr("PGN database (*.pgn)"));
	if (file.isEmpty())
		return;
	if (!file.endsWith(".pgn"))
		file += ".pgn";
	QFile pgnfile(file);
	if (!pgnfile.open(QIODevice::WriteOnly)) 
		MessageDialog::warning(this, tr("Cannot create ChessX database."), tr("New database"));
	else {
		pgnfile.close();
		openDatabase(file);
		AppSettings->setValue("/General/databasePath",
				QFileInfo(file).absolutePath());
	}
}

void MainWindow::slotFileOpen()
{
	QString file = QFileDialog::getOpenFileName(this, tr("Open database"),
			AppSettings->value("/General/databasePath").toString(),
			tr("PGN databases (*.pgn)"));
	if (!file.isEmpty()) {
		AppSettings->setValue("/General/databasePath", QFileInfo(file).absolutePath());
		openDatabase(file);
	}
}

void MainWindow::slotFileOpenRecent()
{
	QAction *action = qobject_cast<QAction *>(sender());
	if (action)
		openDatabase(action->data().toString());
}


void MainWindow::slotFileSave()
{
	if (m_currentDatabase && dynamic_cast<MemoryDatabase*>(database())) {
		Output output(Output::Pgn);
		output.output(database()->filename(), *database());
		slotStatusMessage(tr("Database %1 successfully saved.")
				  .arg(database()->filename().section('/', -1)));
	}
}

void MainWindow::slotFileClose()
{
	if (m_currentDatabase) {// Don't remove Clipboard
		m_databases.removeAt(m_currentDatabase);
		if (m_currentDatabase == m_databases.count())
			m_currentDatabase--;
		updateMenuDatabases();
		slotDatabaseChanged();
	}
}

void MainWindow::slotFileExportFilter()
{
	int format;
	QString filename = exportFileName(format);
	if (!filename.isEmpty()) {
		Output output((Output::OutputType)format);
		output.output(filename, *(databaseInfo()->filter()));
	}
}

void MainWindow::slotFileExportAll()
{
	int format;
	QString filename = exportFileName(format);
	if (!filename.isEmpty()) {
		Output output((Output::OutputType)format);
		output.output(filename, *database());
	}
}

void MainWindow::slotFileQuit()
{
	qApp->closeAllWindows();
}

void MainWindow::slotPlayerDialog()
{
	playerDialog()->setDatabase(database());
	playerDialog()->show();
}

void MainWindow::slotConfigure()
{
	PreferencesDialog P(this);
	connect(&P, SIGNAL(reconfigure()), SLOT(slotReconfigure()));
	P.exec();
}

void MainWindow::slotReconfigure()
{
	// Re-emit for children
	emit reconfigure();
}

void MainWindow::slotConfigureFlip()
{
	m_boardView->setFlipped(!m_boardView->isFlipped());
}

void MainWindow::slotEditCopyFEN()
{
	QApplication::clipboard()->setText(game().toFen());
}

void MainWindow::slotEditPasteFEN()
{
	QString fen = QApplication::clipboard()->text().trimmed();
	Board board;
	if (!board.isValidFen(fen)) {
		QString msg = fen.length() ?
			      tr("Text in clipboard does not represent valid FEN:<br><i>%1</i>").arg(fen) :
			      tr("There is no text in clipboard.");
		MessageDialog::warning(this, msg);
		return;
	}
	board.fromFen(fen);
	if (board.validate() != Valid) {
		MessageDialog::warning(this, tr("The clipboard contains FEN, but with illegal position. "
						"You can only paste such positions in <b>Setup position</b> dialog."));
		return;
	}
	game().setStartingBoard(board);
	slotGameChanged();
}

void MainWindow::slotEditTruncateEnd()
{
	game().truncateVariation(Game::AfterMove);
	slotGameChanged();
}

void MainWindow::slotEditTruncateStart()
{
	game().truncateVariation(Game::BeforeMove);
	slotGameChanged();
}

void MainWindow::slotEditBoard()
{
	BoardSetupDialog B(this);
	B.setBoard(game().board());
	B.setFlipped(m_boardView->isFlipped());
	if (B.exec() == QDialog::Accepted) {
		game().setStartingBoard(B.board());
		slotGameChanged();
	}
}


void MainWindow::slotHelp()
{
	helpWindow()->show();
}

void MainWindow::slotHelpTip()
{
	tipDialog()->show();
}

void MainWindow::slotHelpAbout()
{
	QString fastbits = Board::fastbitsOption ? tr("<br>Compiled with 'fastbits' option") : "";
	QMessageBox dialog(tr(""), tr("<h1>ChessX</h1>"
				      "<p>Free chess database available under GPLv2.<br>Version %1%2<br>"
				      "Copyright 2005-2009 ChessX developers"
				      "<p>Current developer and maintainer: Michal Rudolf <a href=\"mailto:mrudolf"
				      "@kdewebdev.org\">&lt;mrudolf"
				      "@kdewebdev.org&gt;</a>"
				      "<p>Additional coding: Marius Roets, Sean Estabrooks, Rico Zenklusen, "
				      "Ejner Borgbjerg, Heinz Hopfgartner, William Hoggarth."
				      "<p>Homepage: <a href=\"http://chessx.sf.net\">http://chessx.sf.net</a><br>"
				      "Mailing list: <a href=\"mailto:chessx-users@lists.sourceforge.net\">"
				      "chessx-users@lists.sourceforge.net").arg(ChessXVersion).arg(fastbits),
			   QMessageBox::NoIcon, QMessageBox::Ok, Qt::NoButton, Qt::NoButton, this);
	dialog.exec();
}

void MainWindow::slotHelpBug()
{
	QDesktopServices::openUrl(QUrl("http://sourceforge.net/tracker/?group_id=163833&atid=829300"));
}


void MainWindow::slotBoardMove(Square from, Square to)
{
	const Board& board = game().board();
	Move m(board.prepareMove(from, to));
	if (m.isLegal()) {
		if (m.isPromotion()) {
			bool ok;
			QStringList moves;
			moves << tr("Queen") << tr("Rook") << tr("Bishop") << tr("Knight");
			int index = moves.indexOf(QInputDialog::getItem(0, tr("Promotion"), tr("Promote to:"),
						  moves, 0, false, &ok));
			if (!ok)
				return;
			m.setPromotionPiece(PieceType(Queen + index));
		}
		if (game().atLineEnd())
			game().addMove(m);
		else {
			// Find how way we should add new move
			QMessageBox mbox(QMessageBox::Question, tr("Add move"),
					 tr("There is already next move in current game. What do you want to do?"), QMessageBox::Cancel, this);
			QPushButton* addVar = mbox.addButton(tr("Add variation"), QMessageBox::YesRole);
			QPushButton* newMain = mbox.addButton(tr("Add new mainline"), QMessageBox::AcceptRole);
			QPushButton* replaceMain = mbox.addButton(tr("Replace current move"), QMessageBox::DestructiveRole);
			mbox.exec();
			if (mbox.clickedButton() == addVar)
				game().addVariation(m);
			else if (mbox.clickedButton() == newMain)
				game().promoteVariation(game().addVariation(m));
			else if (mbox.clickedButton() == replaceMain)
				game().replaceMove(m);
			else return;
		}
		game().forward();
		slotGameChanged();
	}
}

void MainWindow::slotBoardClick(Square, int button)
{
	if (button != Qt::RightButton)
		return;
	bool remove = game().atLineEnd();
	int var = game().variationNumber();
	gameMoveBy(-1);
	if (remove) {
		if (var && game().isMainline())
			game().removeVariation(var);
		else
			game().truncateVariation();
		slotGameChanged();
	}
}

void MainWindow::slotMoveChanged()
{
	Game& g = game();

	// Set board first
	m_tablebase->abortLookup();
	m_boardView->setBoard(g.board());

	// Highlight current move
	m_gameView->showMove(g.currentMove());

	// Finally update game information
	QString white = g.tag("White");
	QString black = g.tag("Black");
	QString eco = m_eco.isNull() ? g.tag("ECO") : m_eco;
	if (!eco.isEmpty()) {
		int comma = eco.lastIndexOf(',');
		if (comma != -1 && eco.at(comma + 2).isNumber())
			eco.truncate(comma);
	}
	QString whiteElo = g.tag("WhiteElo");
	QString blackElo = g.tag("BlackElo");
	if (whiteElo == "?")
		whiteElo = QString();
	if (blackElo == "?")
		blackElo = QString();
	QString players = tr("Game %1: <b><a href=\"tag:white\">%2</a> %3 - <a href=\"tag:black\">%4</a> %5</b>")
			  .arg(gameIndex() + 1).arg(white).arg(whiteElo).arg(black).arg(blackElo);
	QString result = tr("%1(%2) %3").arg(g.tag("Result")).arg((g.plyCount() + 1) / 2)
			 .arg(eco);
	QString header = tr("<i>%1(%2), %3, %4</i>").arg(g.tag("Event")).arg(g.tag("Round"))
			 .arg(g.tag("Site")).arg(g.tag("Date"));
	QString lastmove, nextmove;
	if (!g.atGameStart())
		lastmove = QString("<a href=\"move:prev\">%1</a>").arg(g.moveToSan(Game::FullDetail, Game::PreviousMove));
	else
		lastmove = tr("(Start of game)");
	if (!g.atGameEnd())
		nextmove = QString("<a href=\"move:next\">%1</a>").arg(g.moveToSan(Game::FullDetail, Game::NextMove));
	else
		nextmove = g.isMainline() ? tr("(End of game)") : tr("(End of line)");
	QString move = tr("Last move: %1 &nbsp; &nbsp; Next: %2").arg(lastmove).arg(nextmove);
	if (!g.isMainline())
		move.append(QString(" &nbsp; &nbsp; <a href=\"move:exit\">%1</a>").arg(tr("(&lt;-Var)")));
	QString var;
	if (g.variationCount()) {
		var = tr("<br>Variations: &nbsp; ");
		QList <int> variations = g.variations();
		for (int i = 1; i <= variations.size(); i++) {
			var.append(QString("v%1: <a href=\"move:%2\">%3</a>").arg(i).arg(variations[i-1])
				   .arg(g.moveToSan(Game::FullDetail, Game::PreviousMove, variations[i-1])));
			if (i != variations.size())
				var.append(" &nbsp; ");
		}
	}
	m_moveView->setText(QString("<qt>%1<br>%2<br>%3<br>%4%5</qt>").arg(players).arg(result)
			    .arg(header).arg(move).arg(var));
	if (AppSettings->value("/General/onlineTablebases", true).toBool())
		m_tablebase->getBestMove(g.toFen());

	slotSearchTree();
	emit boardChange(g.board());
}

void MainWindow::showTablebaseMove(Move move, int score)
{
	QString result;
	if (score < 0)
		result = tr("Loses in %n move(s)", "", score * -1);
	else if (score > 0)
		result = tr("Wins in %n move(s)", "", score);
	else
		result = tr("Draw");

	QString san(m_boardView->board().moveToSan(move));
	QString update = m_moveView->toHtml();
	int s = update.lastIndexOf("</p>");
	update.insert(s, tr("<br>Tablebase: <a href=\"egtb:%1\">%2%3 %1</a> -- %4")
		      .arg(san).arg(game().moveNumber())
		      .arg(game().board().toMove() == White ? "." : "...").arg(result));
	m_moveView->setHtml(update);
}

void MainWindow::slotBoardMoveWheel(int wheel)
{
	if (wheel & Qt::AltModifier)
		if (wheel & BoardView::WheelDown) slotGameMoveLast();
		else slotGameMoveFirst();
	else if (wheel & Qt::ControlModifier)
		if (wheel & BoardView::WheelDown) slotGameMoveNextN();
		else slotGameMovePreviousN();
	else
		if (wheel & BoardView::WheelDown) slotGameMoveNext();
		else slotGameMovePrevious();
}

void MainWindow::slotGameLoadFirst()
{
	gameLoad(databaseInfo()->filter()->indexToGame(0));
	m_gameList->setFocus();
}

void MainWindow::slotGameLoadLast()
{
	gameLoad(databaseInfo()->filter()->indexToGame(databaseInfo()->filter()->count() - 1));
	m_gameList->setFocus();
}

void MainWindow::slotGameLoadPrevious()
{
	int game = m_gameList->currentIndex().row();
	game = databaseInfo()->filter()->indexToGame(game);
	game = databaseInfo()->filter()->previousGame(game);
	if (game != -1) {
		m_gameList->selectGame(game);
		m_gameList->setFocus();
		m_pending = PendingLoad(database(), game);
		m_timer->start();
	}
}

void MainWindow::slotGameLoadNext()
{
	int game = m_gameList->currentIndex().row();
	game = databaseInfo()->filter()->indexToGame(game);
	game = databaseInfo()->filter()->nextGame(game);
	if (game != -1) {
		m_gameList->selectGame(game);
		m_gameList->setFocus();
		m_pending = PendingLoad(database(), game);
		m_timer->start();
	}
}

void MainWindow::slotGameLoadPending()
{
	if (m_pending.database == database())
		gameLoad(m_pending.game);
}


void MainWindow::slotGameLoadRandom()
{
	if (databaseInfo()->filter()->count()) {
		int random = rand() % databaseInfo()->filter()->count();
		gameLoad(databaseInfo()->filter()->indexToGame(random));
		m_gameList->setFocus();
	}
}

void MainWindow::slotGameLoadChosen()
{
	int index = QInputDialog::getInteger(this, tr("Load Game"), tr("Game number:"), gameIndex() + 1,
					     1, database()->count());
	gameLoad(index - 1);
	m_gameList->setFocus();
}

void MainWindow::slotGameNew()
{
	if (database()->isReadOnly())
		MessageDialog::error(this, tr("This database is read only."));
	else {
		databaseInfo()->newGame();
		slotGameChanged();
	}
}

void MainWindow::slotGameSave()
{
	if (database()->isReadOnly())
		MessageDialog::error(this, tr("This database is read only."));
	else if (saveDialog()->exec(database(), game()) == QDialog::Accepted) {
		databaseInfo()->saveGame();
		slotDatabaseChanged();
	}
}

void MainWindow::slotGameModify(int action, int move)
{
	game().moveToId(move);
	slotMoveChanged();
	switch (action) {
	case ChessBrowser::RemoveNextMoves:
		game().truncateVariation();
		break;
	case ChessBrowser::RemovePreviousMoves:
		game().truncateVariation(Game::BeforeMove);
		break;
	case ChessBrowser::RemoveVariation: {
		game().removeVariation(game().variationNumber());
		break;
	}
	case ChessBrowser::EditPrecomment:
		if (!gameEditComment(Output::Precomment))
			return;
		break;
	case ChessBrowser::EditComment:
		if (!gameEditComment(Output::Comment))
			return;
		break;
	default:
		;
	}
	slotGameChanged();
}


void MainWindow::slotGameChanged()
{
	if (m_showPgnSource)
		m_gameView->setPlainText(m_output->output(&game()));
	else
		m_gameView->setText(m_output->output(&game()));
	m_eco = game().ecoClassify();
	slotMoveChanged();
}

void MainWindow::slotGameViewLink(const QUrl& url)
{
	if (url.scheme() == "move") {
		if (url.path() == "prev") game().backward();
		else if (url.path() == "next") game().forward();
		else if (url.path() == "exit") game().moveToId(game().parentMove());
		else
			game().moveToId(url.path().toInt());
		slotMoveChanged();
	} else if (url.scheme() == "precmt" || url.scheme() == "cmt") {
		game().moveToId(url.path().toInt());
		slotMoveChanged();
		Output::CommentType type = url.scheme() == "cmt" ? Output::Comment : Output::Precomment;
		if (gameEditComment(type))
			slotGameChanged();
	} else if (url.scheme() == "egtb") {
		if (!game().atGameEnd())
			game().addVariation(url.path());
		else
			game().addMove(url.path());
		game().forward();
		slotGameChanged();
	} else if (url.scheme() == "tag") {
		playerDialog()->setDatabase(database());
		if (url.path() == "white")
			playerDialog()->showPlayer(game().tag("White"));
		else if (url.path() == "black")
			playerDialog()->showPlayer(game().tag("Black"));
	}
}

void MainWindow::slotGameViewToggle(bool toggled)
{
	m_showPgnSource = toggled;
	slotGameChanged();
}

void MainWindow::slotFilterChanged()
{
	if (gameIndex() >= 0)
		m_gameList->selectGame(gameIndex());
	int count = databaseInfo()->filter()->count();
	QString f = count == database()->count() ? "all" : QString::number(count);
	m_statusFilter->setText(tr(" %1: %2/%3 ").arg(databaseName())
				.arg(f).arg(database()->count()));
}

void MainWindow::slotFilterLoad(int index)
{
	gameLoad(index);
	activateWindow();
}

void MainWindow::slotStatusMessage(const QString& msg)
{
	statusBar()->showMessage(msg);
}

void MainWindow::slotStatusFileOpened(const QString& file)
{
	QFileInfo info(file);
	statusBar()->showMessage(tr("Opening %1...").arg(info.fileName()));
	statusBar()->insertPermanentWidget(0, m_progressBar);
	m_progressBar->setValue(0);
	m_progressBar->show();

}
	
void MainWindow::slotStatusFileClosed(const QString&)
{
	statusBar()->removeWidget(m_progressBar);
}

void MainWindow::slotStatusProgress(int progress)
{
	m_progressBar->setValue(progress);
}


void MainWindow::slotDatabaseChange()
{
	QAction* action = qobject_cast<QAction*>(sender());
	if (action && m_currentDatabase != action->data().toInt()) {
		m_currentDatabase = action->data().toInt();
		slotDatabaseChanged();
	}
}

void MainWindow::slotDatabaseCopy()
{
	if (m_databases.count() < 2) {
		MessageDialog::error(this, tr("You need at least two open databases to copy games"));
		return;
	}
	CopyDialog dlg(this);
	QStringList db;
	for (int i = 0; i < m_databases.count(); i++)
		if (i != m_currentDatabase)
			db.append(tr("%1. %2 (%3 games)").arg(i).arg(databaseName(i))
				  .arg(m_databases[i]->database()->count()));
	dlg.setDatabases(db);
	if (dlg.exec() != QDialog::Accepted)
		return;
	int target = dlg.getDatabase();
	if (target >= m_currentDatabase)
		target++;
	Game g;
	switch (dlg.getMode()) {
	case CopyDialog::SingleGame:
		m_databases[target]->database()->appendGame(game());
		break;
	case CopyDialog::Filter:
		for (int i = 0; i < database()->count(); i++)
			if (databaseInfo()->filter()->contains(i) && database()->loadGame(i, g))
				m_databases[target]->database()->appendGame(g);
		break;
	case CopyDialog::AllGames:
		for (int i = 0; i < database()->count(); i++)
			if (database()->loadGame(i, g))
				m_databases[target]->database()->appendGame(g);
		break;
	default:
		;
	}
	m_databases[target]->filter()->resize(m_databases[target]->database()->count(), 1);
}


void MainWindow::slotDatabaseChanged()
{
	setWindowTitle(tr("ChessX - %1").arg(databaseName()));
	m_gameList->setFilter(databaseInfo()->filter());
	slotFilterChanged();
	gameLoad(gameIndex(), true, true);
	if (m_playerDialog && playerDialog()->isVisible())
		playerDialog()->setDatabase(database());
	
	emit databaseChanged(databaseInfo());
}

QAction* MainWindow::createAction(const QString& name, const char* slot, const QKeySequence& key,
				  const QString& tip)
{
	QAction* action = new QAction(name, m_actions);
	if (!tip.isEmpty())
		action->setStatusTip(tip);
	if (!key.isEmpty())
		action->setShortcut(key);
	if (slot)
		connect(action, SIGNAL(triggered()), slot);
	return action;
}

void MainWindow::setupActions()
{
	/* File menu */
	QMenu* file = menuBar()->addMenu(tr("&File"));
	file->addAction(createAction(tr("&New database..."), SLOT(slotFileNew())));
	file->addAction(createAction(tr("&Open..."), SLOT(slotFileOpen()), Qt::CTRL + Qt::Key_O));
	QMenu* menuRecent = file->addMenu(tr("Open &recent..."));
	const int MaxRecentFiles = 10;
	for (int i = 0; i < MaxRecentFiles; ++i) {
		QAction* action = new QAction(this);
		action->setVisible(false);
		connect(action, SIGNAL(triggered()), SLOT(slotFileOpenRecent()));
		m_recentFileActions.append(action);
		menuRecent->addAction(action);
	}
	file->addAction(createAction(tr("&Save"), SLOT(slotFileSave()), Qt::CTRL + Qt::SHIFT + Qt::Key_S));
	QMenu* exportMenu = file->addMenu(tr("&Export..."));
	exportMenu->addAction(createAction(tr("&Games in filter"), SLOT(slotFileExportFilter())));
	exportMenu->addAction(createAction(tr("&All games"), SLOT(slotFileExportAll())));
	file->addAction(createAction(tr("&Close"), SLOT(slotFileClose()), Qt::CTRL + Qt::Key_W));
	file->addAction(createAction(tr("&Quit"), SLOT(slotFileQuit()), Qt::CTRL + Qt::Key_Q));

	/* Edit menu */
	QMenu* edit = menuBar()->addMenu(tr("&Edit"));
	edit->addAction(createAction(tr("Position &Setup..."), SLOT(slotEditBoard()),
				     Qt::CTRL + Qt::Key_E));
	QMenu* editremove = edit->addMenu(tr("&Remove"));
	editremove->addAction(createAction(tr("Moves from the beginning"),
					   SLOT(slotEditTruncateStart())));
	editremove->addAction(createAction(tr("Moves to the end"), SLOT(slotEditTruncateEnd()),
					   Qt::SHIFT + Qt::Key_Delete));
	edit->addSeparator();
	edit->addAction(createAction(tr("&Copy FEN"), SLOT(slotEditCopyFEN()),
				     Qt::CTRL + Qt::SHIFT + Qt::Key_C));
	edit->addAction(createAction(tr("&Paste FEN"), SLOT(slotEditPasteFEN()),
				     Qt::CTRL + Qt::SHIFT + Qt::Key_V));
	edit->addSeparator();
	edit->addAction(createAction(tr("&Preferences..."), SLOT(slotConfigure())));

	/* Game menu */
	QMenu *gameMenu = menuBar()->addMenu(tr("&Game"));
	QMenu* loadMenu = gameMenu->addMenu(tr("&Load"));

	/* Game->Load submenu */
	loadMenu->addAction(createAction(tr("&First"), SLOT(slotGameLoadFirst()), Qt::CTRL + Qt::SHIFT + Qt::Key_Up));
	loadMenu->addAction(createAction(tr("&Last"), SLOT(slotGameLoadLast()), Qt::CTRL + Qt::SHIFT + Qt::Key_Down));
	loadMenu->addAction(createAction(tr("&Next"), SLOT(slotGameLoadNext()), Qt::CTRL + Qt::Key_Down));
	loadMenu->addAction(createAction(tr("&Previous"), SLOT(slotGameLoadPrevious()), Qt::CTRL + Qt::Key_Up));
	loadMenu->addAction(createAction(tr("&Go to game..."), SLOT(slotGameLoadChosen()), Qt::CTRL + Qt::Key_G));
	loadMenu->addAction(createAction(tr("&Random"), SLOT(slotGameLoadRandom()), Qt::CTRL + Qt::Key_Question));

	/* Game->Go to submenu */
	QMenu* goMenu = gameMenu->addMenu(tr("&Go to"));
	goMenu->addAction(createAction(tr("&Start"), SLOT(slotGameMoveFirst()), Qt::Key_Home));
	goMenu->addAction(createAction(tr("&End"), SLOT(slotGameMoveLast()), Qt::Key_End));
	goMenu->addAction(createAction(tr("&Next move"), SLOT(slotGameMoveNext()), Qt::Key_Right));
	goMenu->addAction(createAction(tr("&Previous move"), SLOT(slotGameMovePrevious()), Qt::Key_Left));
	goMenu->addAction(createAction(tr("5 moves &forward"), SLOT(slotGameMoveNextN()), Qt::Key_Down));
	goMenu->addAction(createAction(tr("5 moves &backward"), SLOT(slotGameMovePreviousN()), Qt::Key_Up));

	gameMenu->addAction(createAction(tr("&New"), SLOT(slotGameNew()), Qt::CTRL + Qt::Key_N));
	gameMenu->addAction(createAction(tr("&Save...."), SLOT(slotGameSave()), Qt::CTRL + Qt::Key_S));

	/* Search menu */
	QMenu* search = menuBar()->addMenu(tr("Fi&nd"));
	search->addAction(createAction(tr("Find &tag"), SLOT(slotSearchTag()), Qt::CTRL +
				       Qt::SHIFT + Qt::Key_T));
	search->addAction(createAction(tr("Find &position"), SLOT(slotSearchBoard()), Qt::CTRL +
				       Qt::SHIFT + Qt::Key_B));
	search->addSeparator();
	search->addAction(createAction(tr("&Reset filter"), SLOT(slotSearchReset()), Qt::CTRL + Qt::Key_F));
	search->addAction(createAction(tr("&Reverse filter"), SLOT(slotSearchReverse()),
				       Qt::CTRL + Qt::SHIFT + Qt::Key_F));

	/* Database menu */
	QMenu* menuDatabase = menuBar()->addMenu(tr("&Database"));
	m_menuDatabases = menuDatabase->addMenu(tr("&Switch to"));
	menuDatabase->addAction(createAction(tr("&Copy games..."), SLOT(slotDatabaseCopy()),
					     Qt::Key_F5));
	QMenu* menuRemove = menuDatabase->addMenu(tr("Delete"));
	menuRemove->addAction(createAction(tr("&Current game"), SLOT(slotDatabaseDeleteGame())));
	menuRemove->addAction(createAction(tr("&Games in filter"), SLOT(slotDatabaseDeleteFilter())));
	menuDatabase->addAction(createAction(tr("&Compact"), SLOT(slotDatabaseCompact())));

	/* View menu */
	m_menuView = menuBar()->addMenu(tr("&View"));
	QAction* flip = createAction(tr("&Flip board"), SLOT(slotConfigureFlip()), Qt::CTRL + Qt::Key_B);
	flip->setCheckable(true);
	m_menuView->addAction(flip);
	m_menuView->addAction(createAction(tr("&Player Database..."), SLOT(slotPlayerDialog()),
					   Qt::CTRL + Qt::SHIFT + Qt::Key_P));


	/* Help menu */
	menuBar()->addSeparator();
	QMenu *help = menuBar()->addMenu(tr("&Help"));
//  help->addAction(createAction(tr("ChessX &help..."), SLOT(slotHelp()), Qt::CTRL + Qt::Key_F1));
	help->addAction(createAction(tr("&Tip of the day"), SLOT(slotHelpTip())));
	help->addAction(createAction(tr("&Report a bug..."), SLOT(slotHelpBug())));
	help->addSeparator();
	help->addAction(createAction(tr("&About ChessX"), SLOT(slotHelpAbout())));

#ifdef QT_DEBUG
	QMenu* debug = help->addMenu(tr("&Debug"));
	QAction* source;
	debug->addAction(source = createAction("Toggle game view format", 0, Qt::Key_F12));
	source->setCheckable(true);
	connect(source, SIGNAL(toggled(bool)), SLOT(slotGameViewToggle(bool)));
#endif
}

void MainWindow::slotSearchTag()
{
	m_gameList->simpleSearch(1);
}

void MainWindow::slotSearchBoard()
{
	PositionSearch ps(databaseInfo()->filter()->database(), m_boardView->board());

	databaseInfo()->filter()->executeSearch(ps);
	m_gameList->updateFilter();
	slotFilterChanged();
}

void MainWindow::slotSearchReverse()
{
	databaseInfo()->filter()->reverse();
	m_gameList->updateFilter();
	slotFilterChanged();
}

void MainWindow::slotSearchReset()
{
	databaseInfo()->filter()->setAll(1);
	m_gameList->updateFilter();
	slotFilterChanged();
}

void MainWindow::slotSearchTree()
{
	if (!m_openingTree->isVisible())
		return;
	QTime time;
	time.start();
	dynamic_cast<OpeningTree*>(m_openingTree->model())->update(*databaseInfo()->filter(), m_boardView->board());
	m_gameList->updateFilter();
	slotFilterChanged();
	slotStatusMessage(tr("Tree updated (%1 s.)").arg(time.elapsed() / 100 / 10.0));
}

void MainWindow::slotSearchTreeMove(const QModelIndex& index)
{
	QString move = dynamic_cast<OpeningTree*>(m_openingTree->model())->move(index);
	Move m = m_boardView->board().parseMove(move);
	if (!m.isLegal())
		return;
	else if (m == game().move(game().nextMove()))
		slotGameMoveNext();
	else if (game().isModified())
		slotBoardMove(m.from(), m.to());
	else {
		Board board = m_boardView->board();
		board.doMove(m);
		m_boardView->setBoard(board);
		slotSearchTree();
		slotGameLoadFirst();
	}
}

void MainWindow::slotDatabaseDeleteGame()
{
	database()->remove(gameIndex());
	m_gameList->updateFilter();
}

void MainWindow::slotDatabaseDeleteFilter()
{
	database()->remove(*databaseInfo()->filter());
	m_gameList->updateFilter();
}

void MainWindow::slotDatabaseCompact()
{
	database()->compact();
	databaseInfo()->resetFilter();
	slotDatabaseChanged();
	m_gameList->updateFilter();
}

bool MainWindow::confirmQuit() 
{
	if (AppSettings->value("/General/confirmQuit", true).toBool() &&
			!MessageDialog::okCancel(this, tr("Do you want to quit?"), tr("Quit"), tr("Quit")))
		return false;
	QString modified;
	for (int i = 1; i < m_databases.size(); i++)
		if (m_databases[i]->database()->isModified())
			modified += m_databases[i]->database()->name() + '\n';
	if (!modified.isEmpty()) {
		int response = MessageDialog::yesNoCancel(this, tr("Following databases are modified:")
					+ '\n' + modified + tr("Save them?"));
		if (response == MessageDialog::Cancel)
			return false;
		Output output(Output::Pgn);
		for (int i = 1; i < m_databases.size(); i++)
			if (m_databases[i]->database()->isModified())
				output.output(m_databases[i]->database()->filename(), 
						*(m_databases[i]->database()));
	}
	return true;
}
