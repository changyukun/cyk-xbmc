/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "PlayList.h"
#include "PlayListFactory.h"
#include <sstream>
#include "video/VideoInfoTag.h"
#include "music/tags/MusicInfoTag.h"
#include "filesystem/File.h"
#include "utils/log.h"
#include "utils/URIUtils.h"
#include "utils/Variant.h"

//using namespace std;
using namespace MUSIC_INFO;
using namespace XFILE;
using namespace PLAYLIST;

CPlayList::CPlayList(void)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	m_strPlayListName = "";
	m_iPlayableItems = -1;
	m_bShuffled = false;
	m_bWasPlayed = false;
}

CPlayList::~CPlayList(void)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
  	Clear();
}

void CPlayList::Add(const CFileItemPtr &item, int iPosition, int iOrder)
{
/*
	参数:
		1、item		: 传入要插入到item 容器内的item 指针
		2、iPosition	: 传入要插入到容器中的位置，即序号
		3、iOrder	: 传入一个item 排序的值，此值会经过修正后存入到item 数据结构中的
		
	返回:
		1、
		
	说明:
		1、添加的原则:
			
*/
	int iOldSize = size(); /* 取出容器内的元素个数*/
	
	if (iPosition < 0 || iPosition >= iOldSize)
		iPosition = iOldSize;
	
	if (iOrder < 0 || iOrder >= iOldSize)
		item->m_iprogramCount = iOldSize;
	else
		item->m_iprogramCount = iOrder;

	// videodb files are not supported by the filesystem as yet
	if (item->IsVideoDb())
		item->SetPath(item->GetVideoInfoTag()->m_strFileNameAndPath);

	// increment the playable counter
	item->ClearProperty("unplayable"); /* 清除不可播放属性*/

	/* 增加item 统计个数*/
	if (m_iPlayableItems < 0)
		m_iPlayableItems = 1;
	else
		m_iPlayableItems++;

	// set 'IsPlayable' property - needed for properly handling plugin:// URLs
	item->SetProperty("IsPlayable", true);/* 设定此item 的可播放属性为真*/

	//CLog::Log(LOGDEBUG,"%s item:(%02i/%02i)[%s]", __FUNCTION__, iPosition, item->m_iprogramCount, item->GetPath().c_str());
	if (iPosition == iOldSize)
		m_vecItems.push_back(item);
	else
	{
		ivecItems it = m_vecItems.begin() + iPosition;
		m_vecItems.insert(it, 1, item);
		// correct any duplicate order values
		if (iOrder < iOldSize)
			IncrementOrder(iPosition + 1, iOrder);
	}
}

void CPlayList::Add(const CFileItemPtr &item)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、添加一个item  到m_vecItems  容器中
*/
  	Add(item, -1, -1);
}

void CPlayList::Add(CPlayList& playlist)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、添加一个播放列表到容器中，实质也是遍历传入的列表
			每个单元，然后将每个单元都插入到m_vecItems  容器中
*/
	for (int i = 0; i < (int)playlist.size(); i++)
		Add(playlist[i], -1, -1);
}

void CPlayList::Add(CFileItemList& items)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、添加一个item 列表到容器中，实质也是遍历传入的列表
			每个单元，然后将每个单元都插入到m_vecItems  容器中

		2、注意此函数与上面重载函数的区别，此函数的参数是
			item 列表，上面的则是播放列表
*/
	for (int i = 0; i < (int)items.Size(); i++)
		Add(items[i]);
}

void CPlayList::Insert(CPlayList& playlist, int iPosition /* = -1 */)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、实质就是将传入的播放列表插入到m_vecItems  容器中的
			iPosition  位置之后
*/
	// out of bounds so just add to the end
	int iSize = size();
	if (iPosition < 0 || iPosition >= iSize)
	{
		Add(playlist);
		return;
	}
	for (int i = 0; i < (int)playlist.size(); i++)
	{
		int iPos = iPosition + i;
		Add(playlist[i], iPos, iPos);
	}
}

void CPlayList::Insert(CFileItemList& items, int iPosition /* = -1 */)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、实质就是将传入的item  列表插入到m_vecItems  容器中的
			iPosition  位置之后
*/
	// out of bounds so just add to the end
	int iSize = size();

	if (iPosition < 0 || iPosition >= iSize)
	{
		Add(items);
		return;
	}
	for (int i = 0; i < (int)items.Size(); i++)
	{
		Add(items[i], iPosition + i, iPosition + i);
	}
}

void CPlayList::Insert(const CFileItemPtr &item, int iPosition /* = -1 */)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、实质就是将传入的 一个item  插入到m_vecItems  容器中的
			iPosition  位置
*/
	// out of bounds so just add to the end
	int iSize = size();
	if (iPosition < 0 || iPosition >= iSize)
	{
		Add(item);
		return;
	}
	Add(item, iPosition, iPosition);
}

void CPlayList::DecrementOrder(int iOrder)
{
/*
	参数:
		1、iOrder	: 传入一个数值，见说明
		
	返回:
		1、
		
	说明:
		1、此函数的实质就是遍历容器m_vecItems 中的所有的单元，只要
			单元中的m_iprogramCount 值大于传入的iOrder  的值，就将此单元
			的m_iprogramCount 值进行减1  操作
*/
	if (iOrder < 0) 
		return;

	// it was the last item so do nothing
	if (iOrder == size())
		return;

	// fix all items with an order greater than the removed iOrder
	ivecItems it;
	it = m_vecItems.begin();
	while (it != m_vecItems.end())
	{
		CFileItemPtr item = *it;
		if (item->m_iprogramCount > iOrder)
		{
			//CLog::Log(LOGDEBUG,"%s fixing item at order %i", __FUNCTION__, item->m_iprogramCount);
			item->m_iprogramCount--;
		}
		++it;
	}
}

void CPlayList::IncrementOrder(int iPosition, int iOrder)
{
/*
	参数:
		1、iPosition	: 传入一个序号，即从容器m_vecItems 开始的序号
		2、iOrder	: 传入一个数值，见说明
		
	返回:
		1、
		
	说明:
		1、此函数的实质就是从容器m_vecItems 中位置为iPosition 的单元开始
			一直到容器最后的所有的单元，只要单元中的m_iprogramCount 
			值大于等于传入的iOrder  的值，就将此单元的m_iprogramCount 
			值进行加1  操作
*/
	if (iOrder < 0) 
		return;

	// fix all items with an order equal or greater to the added iOrder at iPos
	ivecItems it;
	it = m_vecItems.begin() + iPosition;
	while (it != m_vecItems.end())
	{
		CFileItemPtr item = *it;
		if (item->m_iprogramCount >= iOrder)
		{
			//CLog::Log(LOGDEBUG,"%s fixing item at order %i", __FUNCTION__, item->m_iprogramCount);
			item->m_iprogramCount++;
		}
		++it;
	}
}

void CPlayList::Clear()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、清除容器m_vecItems  中所有的单元
*/
	m_vecItems.erase(m_vecItems.begin(), m_vecItems.end());
	m_strPlayListName = "";
	m_iPlayableItems = -1;
	m_bWasPlayed = false;
}

int CPlayList::size() const
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、返回容器内元素的个数
*/
  	return (int)m_vecItems.size();
}

const CFileItemPtr CPlayList::operator[] (int iItem) const
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (iItem < 0 || iItem >= size())
	{
		assert(false);
		CLog::Log(LOGERROR, "Error trying to retrieve an item that's out of range");
		return CFileItemPtr();
	}
	return m_vecItems[iItem];
}

CFileItemPtr CPlayList::operator[] (int iItem)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (iItem < 0 || iItem >= size())
	{
		assert(false);
		CLog::Log(LOGERROR, "Error trying to retrieve an item that's out of range");
		return CFileItemPtr();
	}
	return m_vecItems[iItem];
}

void CPlayList::Shuffle(int iPosition)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (size() == 0)
		// nothing to shuffle, just set the flag for later
		m_bShuffled = true;
	else
	{
		if (iPosition >= size())
			return;
		
		if (iPosition < 0)
			iPosition = 0;
		
		CLog::Log(LOGDEBUG,"%s shuffling at pos:%i", __FUNCTION__, iPosition);

		ivecItems it = m_vecItems.begin() + iPosition;
		random_shuffle(it, m_vecItems.end());

		// the list is now shuffled!
		m_bShuffled = true;
	}
}

struct SSortPlayListItem
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	static bool PlaylistSort(const CFileItemPtr &left, const CFileItemPtr &right)
	{
		return (left->m_iprogramCount <= right->m_iprogramCount);
	}
};

void CPlayList::UnShuffle()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	sort(m_vecItems.begin(), m_vecItems.end(), SSortPlayListItem::PlaylistSort);
	// the list is now unshuffled!
	m_bShuffled = false;
}

const CStdString& CPlayList::GetName() const
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
  	return m_strPlayListName;
}

void CPlayList::Remove(const CStdString& strFileName)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、实质就是遍历m_vecItems  容器中的所有单元，将单元中文件路径
			与传入参数相等的单元从容器中清除掉，然后调用DecrementOrder
			对容器中其他满足条件的单元进行m_iprogramCount 值的更新
*/
	int iOrder = -1;
	ivecItems it;
	it = m_vecItems.begin();
	while (it != m_vecItems.end() )
	{
		CFileItemPtr item = *it;
		if (item->GetPath() == strFileName)
		{
			iOrder = item->m_iprogramCount;
			it = m_vecItems.erase(it);
			//CLog::Log(LOGDEBUG,"PLAYLIST, removing item at order %i", iPos);
		}
		else
			++it;
	}
	DecrementOrder(iOrder); /* 见此函数的说明*/
}

int CPlayList::FindOrder(int iOrder) const
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	for (int i = 0; i < size(); i++)
	{
		if (m_vecItems[i]->m_iprogramCount == iOrder)
			return i;
	}
	return -1;
}

// remove item from playlist by position
void CPlayList::Remove(int position)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	int iOrder = -1;
	if (position >= 0 && position < (int)m_vecItems.size())
	{
		iOrder = m_vecItems[position]->m_iprogramCount;
		m_vecItems.erase(m_vecItems.begin() + position);
	}
	DecrementOrder(iOrder);
}

int CPlayList::RemoveDVDItems()
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	std::vector <CStdString> vecFilenames;

	// Collect playlist items from DVD share
	ivecItems it;
	it = m_vecItems.begin();
	while (it != m_vecItems.end() )
	{
		CFileItemPtr item = *it;
		if ( item->IsCDDA() || item->IsOnDVD() )
		{
			vecFilenames.push_back( item->GetPath() );
		}
		it++;
	}

	// Delete them from playlist
	int nFileCount = vecFilenames.size();
	if ( nFileCount )
	{
		std::vector <CStdString>::iterator it;
		it = vecFilenames.begin();
		while (it != vecFilenames.end() )
		{
			CStdString& strFilename = *it;
			Remove( strFilename );
			it++;
		}
		vecFilenames.erase( vecFilenames.begin(), vecFilenames.end() );
	}
	return nFileCount;
}

bool CPlayList::Swap(int position1, int position2)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if ((position1 < 0) ||(position2 < 0) ||(position1 >= size()) ||(position2 >= size()))
	{
		return false;
	}

	if (!IsShuffled())
	{
		// swap the ordinals before swapping the items!
		//CLog::Log(LOGDEBUG,"PLAYLIST swapping items at orders (%i, %i)",m_vecItems[position1]->m_iprogramCount,m_vecItems[position2]->m_iprogramCount);
		std::swap(m_vecItems[position1]->m_iprogramCount, m_vecItems[position2]->m_iprogramCount);
	}

	// swap the items
	std::swap(m_vecItems[position1], m_vecItems[position2]);
	return true;
}

void CPlayList::SetUnPlayable(int iItem)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (iItem < 0 || iItem >= size())
	{
		CLog::Log(LOGWARNING, "Attempt to set unplayable index %d", iItem);
		return;
	}

	CFileItemPtr item = m_vecItems[iItem];
	if (!item->GetProperty("unplayable").asBoolean())
	{
		item->SetProperty("unplayable", true);
		m_iPlayableItems--;
	}
}


bool CPlayList::Load(const CStdString& strFileName)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	Clear();
	URIUtils::GetDirectory(strFileName, m_strBasePath);

	CFileStream file;
	if (!file.Open(strFileName))
		return false;

	if (file.GetLength() > 1024*1024)
	{
		CLog::Log(LOGWARNING, "%s - File is larger than 1 MB, most likely not a playlist", __FUNCTION__);
		return false;
	}

	return LoadData(file);
}

bool CPlayList::LoadData(std::istream &stream)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	// try to read as a string
	CStdString data;
#if _MSC_VER > 1500
	std::stringstream _stream(data);
	_stream << stream;
#else
	std::stringstream(data) << stream;
#endif
	return LoadData(data);
}

bool CPlayList::LoadData(const CStdString& strData)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
  	return false;
}


bool CPlayList::Expand(int position)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	CFileItemPtr item = m_vecItems[position];
	std::auto_ptr<CPlayList> playlist (CPlayListFactory::Create(*item.get()));
	if ( NULL == playlist.get())
		return false;

	if(!playlist->Load(item->GetPath()))
		return false;

	// remove any item that points back to itself
	for(int i = 0;i<playlist->size();i++)
	{
		if( (*playlist)[i]->GetPath().Equals( item->GetPath() ) )
		{
			playlist->Remove(i);
			i--;
		}
	}

	if(playlist->size() <= 0)
		return false;

	Remove(position);
	Insert(*playlist, position);
	return true;
}

void CPlayList::UpdateItem(const CFileItem *item)
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、实质就是遍历m_vecItems  容器中的每个单元，如果单元中保存
			的文件路径与传入参数item  中的路径相同的，就用传入的item
			将此单元更新
*/
	if (!item) 
		return;

	for (ivecItems it = m_vecItems.begin(); it != m_vecItems.end(); ++it)
	{
		CFileItemPtr playlistItem = *it;
		if (playlistItem->GetPath() == item->GetPath())
		{
			*playlistItem = *item;
			break;
		}
	}
}

const CStdString& CPlayList::ResolveURL(const CFileItemPtr &item ) const
{
/*
	参数:
		1、
		
	返回:
		1、
		
	说明:
		1、
*/
	if (item->IsMusicDb() && item->HasMusicInfoTag())
		return item->GetMusicInfoTag()->GetURL();
	else
		return item->GetPath();
}
