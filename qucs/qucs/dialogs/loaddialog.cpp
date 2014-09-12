/***************************************************************************
 * Copyright (C) 2014 Guilherme Brondani Torri <guitorri@gmail.com>        *
 *                                                                         *
 * Modified from SaveDialog and LibraryDialog                              *
 *                                                                         *
 * This is free software; you can redistribute it and/or modify            *
 * it under the terms of the GNU General Public License as published by    *
 * the Free Software Foundation; either version 2, or (at your option)     *
 * any later version.                                                      *
 *                                                                         *
 * This software is distributed in the hope that it will be useful,        *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of          *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the           *
 * GNU General Public License for more details.                            *
 *                                                                         *
 * You should have received a copy of the GNU General Public License       *
 * along with this package; see the file COPYING.  If not, write to        *
 * the Free Software Foundation, Inc., 51 Franklin Street - Fifth Floor,   *
 * Boston, MA 02110-1301, USA.                                             *
 ***************************************************************************/

#include <QVariant>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QDebug>
#include <QFileDialog>
#include <QScriptEngine>
#include <QScriptValue>
#include <QScriptValueIterator>
#include <QGroupBox>
#include <QListWidget>
#include <QListWidgetItem>
#include <QEvent>
#include <QKeyEvent>
#include <QMessageBox>

#include "loaddialog.h"
#include "qucs.h"
#include "qucsdoc.h"
#include "components/components.h"

LoadDialog::LoadDialog( QWidget* parent )
   : QDialog( parent )
{
   setWindowTitle( tr( "Load Verilog-A symbols" ) );
   app = 0l;
//   initDialog();
}

LoadDialog::~LoadDialog()
{
}

void LoadDialog::setApp(QucsApp *a)
{
   app = a;
}

void LoadDialog::initDialog()
{
   QVBoxLayout *all = new QVBoxLayout(this);
   all->setMargin(5);
   all->setSpacing(6);

   // hold group of files / group icon and checkboxes
   QHBoxLayout *hGroups = new QHBoxLayout();
   all->addLayout(hGroups);

   // ---
   QGroupBox *group1 = new QGroupBox( tr( "Choose Verilog-A symbol files:" ) );
   hGroups->addWidget(group1);

   QScrollArea *scrollArea = new QScrollArea(group1);
   scrollArea->setWidgetResizable(true);

   fileView = new QListWidget(this);

   scrollArea->setWidget(fileView);

   QVBoxLayout *areaLayout = new QVBoxLayout();
   areaLayout->addWidget(scrollArea);
   group1->setLayout(areaLayout);


   // ...........................................................
   QGridLayout *gridButts = new QGridLayout();
   all->addLayout(gridButts);

   ButtSelectAll = new QPushButton(tr("Select All"));
   gridButts->addWidget(ButtSelectAll, 0, 0);
   connect(ButtSelectAll, SIGNAL(clicked()), SLOT(slotSelectAll()));
   ButtSelectNone = new QPushButton(tr("Deselect All"));
   gridButts->addWidget(ButtSelectNone, 0, 1);
   connect(ButtSelectNone, SIGNAL(clicked()), SLOT(slotSelectNone()));
   // ...........................................................
   ButtCancel = new QPushButton(tr("Cancel"));
   gridButts->addWidget(ButtCancel, 1, 0);
   connect(ButtCancel, SIGNAL(clicked()), SLOT(reject()));
   ButtOk = new QPushButton(tr("Ok"));
   gridButts->addWidget(ButtOk, 1, 1);
   connect(ButtOk, SIGNAL(clicked()), SLOT(loadSelected()));
   ButtOk->setDefault(true);


   QVBoxLayout *iconLayout = new QVBoxLayout();

   QGroupBox *group2 = new QGroupBox( );

   QVBoxLayout *group2Layout = new QVBoxLayout();

   iconPixmap = new QLabel();
   iconPixmap->setSizePolicy(QSizePolicy::Expanding,
                             QSizePolicy::Expanding);
   iconPixmap->setAlignment(Qt::AlignCenter);
   group2Layout->addWidget(iconPixmap);

   group2->setLayout(group2Layout);
   iconLayout->addWidget(group2);

   ButtChangeIcon = new QPushButton(tr("Change Icon"));
   iconLayout->addWidget(ButtChangeIcon);
   hGroups->addLayout(iconLayout);
   connect(ButtChangeIcon,SIGNAL(clicked()),this,SLOT(slotChangeIcon()));

   QHBoxLayout *HincTextLayout = new QHBoxLayout();
   labelInclude = new QLabel(tr("Include:"));
   HincTextLayout->addWidget(labelInclude);
   lineInclude = new QLineEdit;
   HincTextLayout->addWidget(lineInclude);
   iconLayout->addLayout(HincTextLayout);
   lineInclude->setDisabled(true);

   QHBoxLayout *HincButLayout = new QHBoxLayout();
   ButtInclude = new QPushButton(tr("Include"));
   ButtInclude->setToolTip(tr("Include SPICE .model parameters"));
   HincButLayout->addWidget(ButtInclude);
   connect(ButtInclude,SIGNAL(clicked()),this,SLOT(slotInclude()));

   ButtRemove = new QPushButton(tr("Remove"));
   ButtRemove->setToolTip(tr("Remove SPICE .model parameters"));
   HincButLayout->addWidget(ButtRemove);
   connect(ButtRemove,SIGNAL(clicked()),this,SLOT(slotRemove()));

   iconLayout->addLayout(HincButLayout);

   // group checkboxes
   QGroupBox *group3 = new QGroupBox();
   QVBoxLayout *group3Layout = new QVBoxLayout();
   group3->setLayout(group3Layout);
   iconLayout->addWidget(group3);

   //
   QCheckBox *autoLoadSelCheck = new QCheckBox(tr("auto-load selected"));
   autoLoadSelCheck->setToolTip(
               tr("Load the selected symbols when opening the project."));

   autoLoadSelCheck->setDisabled(true); //disabled for now

   group3Layout->addWidget(autoLoadSelCheck);

   /*
   QCheckBox *autoLoadAllwaysCheck = new QCheckBox(tr("auto-load all"));
   autoLoadAllwaysCheck->setToolTip(
               tr("Load all symbols."));
   group3Layout->addWidget(autoLoadAllwaysCheck);
   */

   connect(fileView, SIGNAL(itemPressed(QListWidgetItem*)),
           this, SLOT(slotSymbolFileClicked(QListWidgetItem*)));

//   qDebug() << "files " << symbolFiles;

   for(int i=0; i < propsListFiles.size(); i++){
     QListWidgetItem *item = new QListWidgetItem(propsListFiles.at(i), fileView);
     item->setFlags( item->flags() | Qt::ItemIsUserCheckable );
     item->setCheckState(Qt::Checked);

     //set first as selected, one need to be selected to assign bitmap
     fileView->setCurrentRow(0);


     // populate
     QString JSON = projDir.filePath(item->text());

     QString data = getData(JSON);
     QScriptEngine engine;
     QScriptValue vadata = engine.evaluate("(" + data + ")");

     QString module = getString(vadata, "Model");
     QString IncludeFile = getString(vadata, "IncludeFile");

     ButtRemove->setEnabled(false);
     if (!IncludeFile.isEmpty()) {
       moduleInclude[module] = IncludeFile;
       ButtRemove->setEnabled(true);
     }
   }

   // update icon
   this->slotSymbolFileClicked(fileView->currentItem());

   fileView->installEventFilter(this);
   fileView->setFocus();

}


/* Load props, if include, do replacement
 * Load sym, append to props
 * Save as _symbol.json
 *
 *
 */
void LoadDialog::createSymbol(QString module)
{

  qDebug() << "  Creating _symbol.json for:" << module ;


  // if it is a file, put it on the

  // load file, read line by line
  // parse +
  // split =
  // trim
  // store key, value

  /*
    At this point "_sym.json" and "_props.json" are already created.
    They were created during save text symbol.
  */

  /*!
   *   Load include parameters
   */

  // dictionary to store parameters (key, value)  for the include model file
  QMap<QString, QString> includeParam;

  qDebug() << "  Have includes for:" << moduleInclude.keys();

  // only do replacement if needed
  if (moduleInclude.contains(module)) {

      qDebug() << "  Load include parameters, module:" << module;

      // get include for this module
      QString includeFileName = projDir.absoluteFilePath(moduleInclude[module]);

      QFile incFile(includeFileName);
      if (!incFile.open(QIODevice::ReadOnly | QIODevice::Text)){
          QMessageBox::critical(this, tr("Error"),
                                tr("File not found: %1").arg(includeFileName));
        }
      else {
          QTextStream in(&incFile);
          while ( !in.atEnd() ) {
            QString line = in.readLine();

            /// \todo read model name, check consistency
            if (line.contains(".model")) {
                qDebug() << "  model include:" <<line;
              }
            if (line.contains("+")) {

                QStringList KeyVal = line.section("+",1).split("=");

                QString Key = KeyVal[0].stripWhiteSpace();
                QString Val = KeyVal[1].stripWhiteSpace();
                // qDebug() << Key << Val;
                includeParam[Key] = Val;
              }
            }
      }
      incFile.close();

      qDebug() << "  Include Parameters:" << includeParam.keys().size();
  }


  /*!
   *   load default properties
   */

  // current module JSON _props file
  QString propslJSON = projDir.filePath(module+"_props.json");

  qDebug() << "  Loading original props" << propslJSON;

  // dictionary to store parameters (key, value) for the original model.
  QHash<QString, QString> originalParam;

  // only do replacement if needed
  if (moduleInclude.contains(module)) {

      qDebug() << "  Load original parameters, module:" << module;

      QString data = getData(propslJSON);
      QScriptEngine engine;
      QScriptValue vadata = engine.evaluate("(" + data + ")");

      QScriptValue jsonProps = vadata.property("property");
      QScriptValueIterator it(jsonProps);


      while (it.hasNext()) {
          it.next();

          QScriptValue entry = it.value();

          // skip length named iterate
          if (it.name().compare("length")) {
              QString name = getString(entry, "name");
              QString value = getString(entry, "value");

              // add to dictionary of original parameters
              originalParam[name]=value;
            }
      }
      qDebug() << "  Original parameters:" << originalParam.keys().size();
  }


  /* The issue:
   * QStriptengine does not stream out...
   * To handle json creaton properly we need to write a serializer.... argh!
   * Qt5 has a serializer builtin... maybe we can wait a little longer?
   */


  // load original _props into memory
  QFile f1 (propslJSON);
  f1.open(QIODevice::ReadOnly | QIODevice::Text);
  QString dat1 = QString(f1.readAll());
  f1.close();


  // modify the contents of _props.json in memory
  // only do replacement if needed
  if (moduleInclude.contains(module)) {

      qDebug() << "  Search/replace include parameters, module:" << module;

      QString origParam;
      QString inclParam;

      QList<QString> Keys = includeParam.keys();

      int replaced = 0;
      foreach(QString key, Keys) {

          // model has the include parameters we want to replace?

          /// \todo make sure the includeParam key if found in defaultParam keys regardlesss of
          /// see modelcard.pmos, params includes are lowercase -> force to uppercase
          /// Need to find a robust way to map includes to original....
          ///

          QString keyUpper = key.toUpper();

          if (originalParam.keys().contains(keyUpper)) {

              // is the value different?
              //          if ( abs (includeParam[Key].toDouble() - defaultParam[Key].toDouble()) > 1e-12 ) {
              if ( originalParam[keyUpper].toDouble() != includeParam[key].toDouble() ) {

                  //increment counter
                  replaced++;

                  /// \todo Should use a JSON serializer here... ugly hack...
                  origParam = QString("\"name\" : \"%1\", \"value\" : \"%2\"").arg(keyUpper).arg(originalParam[keyUpper]);
                  inclParam = QString("\"name\" : \"%1\", \"value\" : \"%2\"").arg(keyUpper).arg(includeParam[key]);

                  qDebug() << "  replace: "
                           << "\t" << keyUpper << originalParam[keyUpper]
                           << "\t << "
                           << "\t" << key << includeParam[key];

                  if (dat1.find(origParam)) {
                      // do replacement
                      dat1.replace(dat1.indexOf(origParam), origParam.size(), inclParam);
                    }
                  /// \todo error if not found...
                }
            }
      } // foreach
      qDebug() << "  Replaced parameters:" << replaced;
  }

  // Append _sym.json into _props.json, save into _symbol.json
  QFile f2(QucsSettings.QucsWorkDir.filePath(module+"_sym.json"));
  f2.open(QIODevice::ReadOnly | QIODevice::Text);
  QString dat2 = QString(f2.readAll());

  // combine _props (modified or not) with _sym
  QString finalJSON = dat1.append(dat2);

  // remove joining point
  finalJSON = finalJSON.replace("}\n{", "");

  QFile f3(QucsSettings.QucsWorkDir.filePath(module+"_symbol.json"));
  f3.open(QIODevice::WriteOnly | QIODevice::Text);
  QTextStream out(&f3);
  out << finalJSON;

  qDebug() << "  Wrote "<< f3.fileName();

  f1.close();
  f2.close();
  f3.close();

}

void LoadDialog::slotSelectAll()
{
    for(int i = 0; i < fileView->count(); ++i)
    {
        QListWidgetItem* item = fileView->item(i);
        item->setCheckState(Qt::Checked);
    }
}

void LoadDialog::slotSelectNone()
{
    for(int i = 0; i < fileView->count(); ++i)
    {
        QListWidgetItem* item = fileView->item(i);
        item->setCheckState(Qt::Unchecked);
    }
}

/*!
 * \brief LoadDialog::slotSymbolFileClicked
 *   Update bitmap and include status for selected model.
 * \param item Current item on the list.
 */
void LoadDialog::slotSymbolFileClicked(QListWidgetItem* item)
{
//  qDebug() << "pressed" << item->text();
 // get bitmap, try to plot
 // similar to QucsApp::slotSetCompView
  QString JSON = projDir.filePath(item->text());

  /// \todo parse _props.json, get
  /// "BitmapFile" : "bsim6NMOS",
  /// "IncludeFile" : "",
  QString data = getData(JSON);
  QScriptEngine engine;
  QScriptValue vadata = engine.evaluate("(" + data + ")");

  // Get name of bitmap
  QString BitmapFile = getString(vadata, "BitmapFile");
  qDebug() << " bitmap  " << BitmapFile;

  // check if icon exists, fall back to default
  QString iconPath = QString(projDir.absoluteFilePath(BitmapFile+".png"));
  QFile iconFile(iconPath);

  // load bitmap defined on the JSON symbol file
  // or use default icon
  if(iconFile.exists())
  {
    iconPixmap->setPixmap(QPixmap(iconPath));
  }
  else
  {
    QMessageBox::information(this, tr("Info"),
                 tr("Icon not found:\n %1.png").arg(BitmapFile));
    iconPixmap->setPixmap(QPixmap(":/bitmaps/editdelete.png"));
    }

  // Get include file if any
  QString IncludeFile = getString(vadata, "IncludeFile");
  qDebug() << " include  " << IncludeFile;

  if (!IncludeFile.isEmpty()) {
    lineInclude->setText(IncludeFile);
  }
  else
    lineInclude->setText("");
}

/*!
 * \brief LoadDialog::slotIncludeModel
 * Creates a list of model -> include for later override
 *
 *  \todo find way to enable/disable include?
 */
void LoadDialog::slotInclude()
{
  qDebug() << "Include what?";

  // get file for inclusion
  QString includeFileName =
          QFileDialog::getOpenFileName(this,
                                        tr("Open File"),
                                        QString(projDir.absolutePath()),
                                        tr("Include model (*.*)"));

  QString newInclude =  QFileInfo(includeFileName).fileName();


  QListWidgetItem* item = fileView->currentItem();
  QString moduleProps = projDir.filePath(item->text());
  QString module = item->text().split("_props.json")[0];

  qDebug() << module << includeFileName << newInclude;

  // keep track of what files that need to be included
  moduleInclude[module] = includeFileName;


  //annotate the _pros.json with the include filename
  /// \todo replace by a json serializer
  QFile file(moduleProps);
  QByteArray ba;
  ba.clear();
  if (!file.open(QIODevice::ReadWrite | QIODevice::Text)){
    QMessageBox::critical(this, tr("Error"),
                          tr("File not found: %1").arg(moduleProps));
  }
  else {
    QTextStream in(&file);
    while ( !in.atEnd() )
    {
      QString line = in.readLine();
      if (line.contains("IncludeFile")){
          qDebug() << line;
          QString change =
                  QString("  \"IncludeFile\" : \"%1\",").arg(newInclude);
          ba.append(change+"\n");
      }
      else{
          ba.append(line+"\n");
      }
    }
  }
  // write back to the same file, clear it first
  file.resize(0);
  file.write(ba);
  file.close();


  lineInclude->setText(newInclude);
  ButtRemove->setEnabled(true);

}

// remove include for current module
void LoadDialog::slotRemove()
{
  QListWidgetItem* item = fileView->currentItem();
  QString moduleProps = projDir.filePath(item->text());
  QString module = item->text().split("_props.json")[0];

  qDebug() << "  Remove include for:"<< module;

  // keep track of what files that need to be included
//  moduleInclude[module] = includeFileName;

  // remove from list
  moduleInclude.remove(module);
  lineInclude->setText("");

  // remove from _props
  /// \todo refactor, used in other places...
  QFile file(moduleProps);
  QByteArray ba;
  ba.clear();
  if (!file.open(QIODevice::ReadWrite | QIODevice::Text)){
    QMessageBox::critical(this, tr("Error"),
                          tr("File not found: %1").arg(moduleProps));
  }
  else {
    QTextStream in(&file);
    while ( !in.atEnd() )
    {
      QString line = in.readLine();
      if (line.contains("IncludeFile")){
          qDebug() << line;
          QString change =
                  QString("  \"IncludeFile\" : \"\",");//.arg(newInclude);
          ba.append(change+"\n");
      }
      else{
          ba.append(line+"\n");
      }
    }
  }
  // write back to the same file, clear it first
  file.resize(0);
  file.write(ba);
  file.close();

}

void LoadDialog::reject()
{
    done(AbortClosing);
}

//
void LoadDialog::loadSelected()
{
  // merge json files for selected items
  //  _sym.json / _props.json (<< include)
  // build list vaComponentds
  // hand it down to main app


  selectedComponents.clear();

  for(int i = 0; i < fileView->count(); ++i)
  {
    QListWidgetItem* item = fileView->item(i);

    if (item->checkState() == Qt::Checked){
        QString key = item->text().split("_props.json").at(0);

        // create _symbol.json
        createSymbol(key);
        QString value = projDir.absoluteFilePath(key+"_symbol.json");

        qDebug() << "key" << key;
        qDebug() << "file " << value;

        // used by Qucs to load the symbol, drag an drop from dock
        selectedComponents[key] = value;
    }
  }

  accept();
}


/*!
 * Browse for icon image
 * Save image path to JSON symbol file
 */
void LoadDialog::slotChangeIcon()
{
  qDebug() << "  + slotChangeIcon";
  QString iconFileName =
          QFileDialog::getOpenFileName(this,
                                        tr("Open File"),
                                        QString(projDir.absolutePath()),
                                        tr("Icon image (*.png)"));

  QString newIcon =  QFileInfo(iconFileName).completeBaseName();
//  qDebug() << "icon "<< newIcon;

  QString filename = fileView->currentItem()->text();
  filename = projDir.absoluteFilePath(filename);
//  qDebug() << "for " <<  filename;

  // open json
  // change property
  // save&close
  // Try to open the JSON file, can use QScriptEngine for this?
  //
  QFile file(filename);
  QByteArray ba;
  ba.clear();
  if (!file.open(QIODevice::ReadWrite | QIODevice::Text)){
    QMessageBox::critical(this, tr("Error"),
                          tr("File not found: %1").arg(filename));
  }
  else {
    QTextStream in(&file);
    while ( !in.atEnd() )
    {
      QString line = in.readLine();
      if (line.contains("BitmapFile")){
          QString change =
                  QString("  \"BitmapFile\" : \"%1\",").arg(newIcon);
          ba.append(change+"\n");
      }
      else{
          ba.append(line+"\n");
      }
    }
  }

  // write back to the same file, clear it first
  file.resize(0);
  file.write(ba);
  file.close();

  // update icon
  this->slotSymbolFileClicked(fileView->currentItem());
}


/*!
 * \brief LoadDialog::eventFilter Handle key arrows navigation on the list.
 * \param obj
 * \param event
 * \return
 */
bool LoadDialog::eventFilter(QObject *obj, QEvent *event)
{
  Q_UNUSED(obj);

  if (event->type() == QEvent::KeyPress) {
    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
//    qDebug() << "type" << keyEvent->key() << fileView->count();
    if (keyEvent->key() == Qt::Key_Up) {

        fileView->setCurrentRow(std::max(0, fileView->currentRow()-1));
        this->slotSymbolFileClicked(fileView->currentItem());
        return true;
    }
    if (keyEvent->key() == Qt::Key_Down) {
        fileView->setCurrentRow(std::min(fileView->count()-1, fileView->currentRow()+1));
        this->slotSymbolFileClicked(fileView->currentItem());
        return true;
    }
  }
  return false;
}
