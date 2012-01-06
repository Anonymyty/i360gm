#include <Windows.h>
#include "Iso.h"

Iso::Iso()
{
	//Make this a threaded objecy
	QThread *_thread = new QThread;
	moveToThread(_thread);
	_thread->start();

	_isValidXex = NULL;
	_defaultXex = NULL;
	_rootFile = NULL;
}

Iso::~Iso()
{
	if(_disc != NULL)
		delete _disc;

	//We should dealloc the bin tree
	_rootFile->deleteTree();
}


void Iso::extractIso(QString output)
{
	//Start extracting every file
	CreateDirectory(output.toStdWString().c_str(), NULL);
	extractFile(output, _rootFile);
	emit doIsoExtracted(this);
}

void Iso::extractFile(QString output, FileNode *node)
{
	string dir = output.toStdString();
	int64 writen = _disc->saveFile(dir, node->file);

	if(writen == 0) //Was a dir so dive into the dir
		extractFile(QString::fromStdString(dir), node->dir);
	emit doFileExtracted(QString::fromAscii((char*)node->file->name, node->file->length), writen);

	if(node->hasLeft())
		extractFile(output, node->left);
	if(node->hasRight())
		extractFile(output, node->right);
}

uint Iso::getFileNo()
{
	return _fileNo;
}

/**
 * getPath returns the current full path of this iso
 * @return QString containing full path
 */
QString Iso::getPath()
{
	return _path;
}

/**
 * getIso returns the full name of the iso file (including .iso)
 * @return Full iso name. Example: blablabla.iso
 */
QString Iso::getIso()
{
	return _path.split("/").back();
}

/**
 * getShortIso returns the name of the iso file (exluding .iso)
 * @return Iso name, example: cool_game
 */
QString Iso::getShortIso()
{
	QString shortIso = getIso(); shortIso.chop(4);
	return shortIso;
}

/**
 * setPath sets the path where to find the iso. The argument needs to be absolute or relative path to the iso. Example: C://test.iso
 * @param path a QString containing the path
 * @return If the file exists returns true, else false
 */
bool Iso::setPath(QString path)
{
	_path = path;

	_disc = new XboxDisc(path.toStdString());
	if(_disc->isValidMedia())
	{
		getRootNode();
		return true;
	}
	return false;
}


/**
 * getRootNode returns the root element of the binary tree of the file structure
 * @return a pointer to the root element
 */
FileNode* Iso::getRootNode()
{
	if(_rootFile == NULL) //Caching system
	{
		SectorData *data = _disc->getRootData();
		if(data->isInit())
		{
			_fileNo = 0;
			startTime();
			makeTree(data, 0, _rootFile);
			_binTree = stopTime();
		}
	}
	return _rootFile;
}

/**
 * makeTree walks the complete iso in a binary tree and recreates the complete file structure.
 * @param sector address that points to the data of the sector we searching in
 * @param offset the offset to read the file info from
 * @param node the node to add the info to
 */
void Iso::makeTree(SectorData *sector, uint offset, FileNode *&node)
{
	XboxFileInfo *fileInfo = _disc->getFileInfo(sector, offset);
	if(fileInfo == NULL)                                           //Wtf load failed, this iso is corrupt or my code sucks!
		return;

	node = new FileNode(fileInfo);
	uint lOffset = fileInfo->getLOffset();
	uint rOffset = fileInfo->getROffset();
	_fileNo++;

	if(_defaultXex == NULL)                                        //While we at it, find default.xex
		if(fileInfo->isEqual(XEX_FILE, XEX_FILE_SIZE))
		{
			_defaultXex = fileInfo;
			defaultXex = new Xex(_disc, _defaultXex);
		}

	if(fileInfo->isDir() && !fileInfo->isEmpty())                  //This is a directory, read the sector and begin
	{
		SectorData *dirSector = _disc->getSector(fileInfo->sector, fileInfo->size);
		if(dirSector->isInit())                                    //Sanity check for allocation
			makeTree(dirSector, 0, node->dir);
	}

	if(lOffset > 0)                                                //Read the other files
		makeTree(sector, lOffset, node->left);
	if(rOffset > 0)
		makeTree(sector, rOffset, node->right);
}

/**
 * getFile returns a pointer to a XboxFileInfo struct for the file with name and length. If there was no such file it returns NULL.
 * @param name contains the file name
 * @param length length of the name
 * @return XboxFileInfo struct containing information for that file
 */
XboxFileInfo* Iso::getFile(char* name, int length)
{
	return FileNode::getXboxByName(getRootNode(), name, length);
}

/**
 * isDefaultXex check is this iso has a valid default xex (or if we can find the default.xex ;)).
 * @return True is is has a valid default.xex, else false
 */
bool Iso::isDefaultXex()
{
	if(_isValidXex == NULL)
	{
		if(_defaultXex == NULL)
			return false;                                              //We could not find the file...
		if(_disc->isXex(_defaultXex))
			_isValidXex = 1;
	}

	return (_isValidXex == 1);
}

XboxDisc *Iso::getDisc()
{
	return _disc;
}

/**
 * getField returns data based on colum number, this is used for the ItemWidget of Qt
 * @param column referencing to what colum we want the data
 * @return QVariant containing data for that column
 */
QVariant Iso::getField(int column)
{
	QString str;
	switch(column)
	{
	case 0:
		return _binTree;
	case 1:
		return _disc->hashTime;
	case 2:
		str = str.sprintf("0x%08X", _disc->getHash());
		return str;
	case 3:
		return getIso();
	case 4:
		return isDefaultXex();
	case 5:
		return QString::fromStdString(defaultXex->getTitleId());
	case 6:
		return QString::fromStdString(fileNameDb[defaultXex->getFullId()]);
	default:
		return QVariant();
	}
}