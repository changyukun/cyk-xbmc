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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	m_strPlayListName = "";
	m_iPlayableItems = -1;
	m_bShuffled = false;
	m_bWasPlayed = false;
}

CPlayList::~CPlayList(void)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	Clear();
}

void CPlayList::Add(const CFileItemPtr &item, int iPosition, int iOrder)
{
/*
	����:
		1��item		: ����Ҫ���뵽item �����ڵ�item ָ��
		2��iPosition	: ����Ҫ���뵽�����е�λ�ã������
		3��iOrder	: ����һ��item �����ֵ����ֵ�ᾭ����������뵽item ���ݽṹ�е�
		
	����:
		1��
		
	˵��:
		1����ӵ�ԭ��:
			
*/
	int iOldSize = size(); /* ȡ�������ڵ�Ԫ�ظ���*/
	
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
	item->ClearProperty("unplayable"); /* ������ɲ�������*/

	/* ����item ͳ�Ƹ���*/
	if (m_iPlayableItems < 0)
		m_iPlayableItems = 1;
	else
		m_iPlayableItems++;

	// set 'IsPlayable' property - needed for properly handling plugin:// URLs
	item->SetProperty("IsPlayable", true);/* �趨��item �Ŀɲ�������Ϊ��*/

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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1�����һ��item  ��m_vecItems  ������
*/
  	Add(item, -1, -1);
}

void CPlayList::Add(CPlayList& playlist)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1�����һ�������б������У�ʵ��Ҳ�Ǳ���������б�
			ÿ����Ԫ��Ȼ��ÿ����Ԫ�����뵽m_vecItems  ������
*/
	for (int i = 0; i < (int)playlist.size(); i++)
		Add(playlist[i], -1, -1);
}

void CPlayList::Add(CFileItemList& items)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1�����һ��item �б������У�ʵ��Ҳ�Ǳ���������б�
			ÿ����Ԫ��Ȼ��ÿ����Ԫ�����뵽m_vecItems  ������

		2��ע��˺������������غ��������𣬴˺����Ĳ�����
			item �б���������ǲ����б�
*/
	for (int i = 0; i < (int)items.Size(); i++)
		Add(items[i]);
}

void CPlayList::Insert(CPlayList& playlist, int iPosition /* = -1 */)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��ʵ�ʾ��ǽ�����Ĳ����б���뵽m_vecItems  �����е�
			iPosition  λ��֮��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��ʵ�ʾ��ǽ������item  �б���뵽m_vecItems  �����е�
			iPosition  λ��֮��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��ʵ�ʾ��ǽ������ һ��item  ���뵽m_vecItems  �����е�
			iPosition  λ��
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
	����:
		1��iOrder	: ����һ����ֵ����˵��
		
	����:
		1��
		
	˵��:
		1���˺�����ʵ�ʾ��Ǳ�������m_vecItems �е����еĵ�Ԫ��ֻҪ
			��Ԫ�е�m_iprogramCount ֵ���ڴ����iOrder  ��ֵ���ͽ��˵�Ԫ
			��m_iprogramCount ֵ���м�1  ����
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
	����:
		1��iPosition	: ����һ����ţ���������m_vecItems ��ʼ�����
		2��iOrder	: ����һ����ֵ����˵��
		
	����:
		1��
		
	˵��:
		1���˺�����ʵ�ʾ��Ǵ�����m_vecItems ��λ��ΪiPosition �ĵ�Ԫ��ʼ
			һֱ�������������еĵ�Ԫ��ֻҪ��Ԫ�е�m_iprogramCount 
			ֵ���ڵ��ڴ����iOrder  ��ֵ���ͽ��˵�Ԫ��m_iprogramCount 
			ֵ���м�1  ����
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1���������m_vecItems  �����еĵ�Ԫ
*/
	m_vecItems.erase(m_vecItems.begin(), m_vecItems.end());
	m_strPlayListName = "";
	m_iPlayableItems = -1;
	m_bWasPlayed = false;
}

int CPlayList::size() const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1������������Ԫ�صĸ���
*/
  	return (int)m_vecItems.size();
}

const CFileItemPtr CPlayList::operator[] (int iItem) const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	if (iItem < 0 || iItem >= size())
	{
		assert(false);
		CLog::Log(LOGERROR, "Error trying to retrieve an item that's out of range");
		return CFileItemPtr();
	}
	return m_vecItems[iItem];
}


/* =================================================================
ʹ��random_shuffle()�㷨���������Ԫ�� 


��������Ҫָ����Χ�ڵ����������ͳ�ķ�����ʹ��ANSI C�ĺ���random(),Ȼ���ʽ������Ա���������ָ���ķ�Χ�ڡ����ǣ�ʹ�������������������ȱ�㡣
    ���ȣ�����ʽ��ʱ�����������Ť���ģ����Եò�����ȷ�����������ĳЩ���ĳ���Ƶ��Ҫ������������
    ��Σ�random()ֻ֧����������������������������ַ������������ַ��������ݿ��еļ�¼��
    �������ϵ��������⣬C++���ṩ�˸��õĽ���������Ǿ���random_shuffle()�㷨����Ҫ�ż��������Ҿͻ����������������㷨��������ͬ���͵������


    ����ָ����Χ�ڵ����Ԫ�ؼ�����ѷ����Ǵ���һ��˳�����У�Ҳ�������������������飩�������˳�������к���ָ����Χ������ֵ�����磬�������Ҫ����100��0-99֮���������ô�ʹ���һ����������100�����������е������������ #include <vector> using std::vector;int main(){ vector<int> vi; for (int i = 0; i < 10; i++) vi.push_back(i); �������������� 100 �� 0-99 ֮����������Ұ���������}    ���������֮����random_shuffle()�㷨����Ԫ������˳��random_shuffle()�����ڱ�׼��ͷ�ļ�<algorithm.h>�С���Ϊ
���е�STL�㷨���������ֿռ�std::�������ģ�������Ҫע����ȷ�������������͡�random_shuffle()��������������һ��������ָ��������Ԫ�صĵ��������ڶ���������ָ���������һ��Ԫ�ص���һ��λ�á����д������random_shuffle()�㷨��������ǰ��䵽�����е�Ԫ�أ� include <algorithm>


���ȼ򵥵Ľ���һ���˿���ϴ�Ƶķ���������һ������ poker[52] �д���һ���˿���1-52���Ƶ�ֵ��ʹ��һ��forѭ������������飬ÿ��ѭ��������һ��[0��52)֮��������RandNum����RandNumΪ�����±꣬�ѵ�ǰ�±��Ӧ��ֵ��RandNum��Ӧλ�õ�ֵ������ѭ��������ÿ���ƶ���ĳ��λ�ý�����һ�Σ�����һ���ƾͱ�������
===================================================================*/

void CPlayList::Shuffle(int iPosition)
{
/*
	����:
		1��iPosition : ����һ�������е�λ�ò���
		
	����:
		1��
		
	˵��:
		1���˺���ʵ���˽������д�λ��iPosition ��ʼ����������
			��Ԫ��������������������random_shuffle  ������
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

		/*
			���������Ԫ�أ����ô˺���֮ǰ�������srand(time(NULL)); ���г�ʼ��
			�������
		*/
		random_shuffle(it, m_vecItems.end());

		// the list is now shuffled!
		m_bShuffled = true;
	}
}

struct SSortPlayListItem
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	static bool PlaylistSort(const CFileItemPtr &left, const CFileItemPtr &right)
	{
		return (left->m_iprogramCount <= right->m_iprogramCount);
	}
};

void CPlayList::UnShuffle()
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��Ӧ�����뷽��Shuffle  �෴�Ĺ���
*/
	sort(m_vecItems.begin(), m_vecItems.end(), SSortPlayListItem::PlaylistSort);
	// the list is now unshuffled!
	m_bShuffled = false;
}

const CStdString& CPlayList::GetName() const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	return m_strPlayListName;
}

void CPlayList::Remove(const CStdString& strFileName)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��ʵ�ʾ��Ǳ���m_vecItems  �����е����е�Ԫ������Ԫ���ļ�·��
			�봫�������ȵĵ�Ԫ���������������Ȼ�����DecrementOrder
			���������������������ĵ�Ԫ����m_iprogramCount ֵ�ĸ���
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
	
	DecrementOrder(iOrder); /* ���˺�����˵��*/
}

int CPlayList::FindOrder(int iOrder) const
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��iItem : ����������һ����Ԫ�����
		
	����:
		1��
		
	˵��:
		1������ʵ���˽�������λ��ΪiItem  ��Ԫ��ubplayable ��������Ϊ�棬����
			��ͳ�ƿɲ��ŵ�Ԫ������1 
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
  	return false;
}


bool CPlayList::Expand(int position)
{
/*
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��ʵ�ʾ��Ǳ���m_vecItems  �����е�ÿ����Ԫ�������Ԫ�б���
			���ļ�·���봫�����item  �е�·����ͬ�ģ����ô����item
			���˵�Ԫ����
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
	����:
		1��
		
	����:
		1��
		
	˵��:
		1��
*/
	if (item->IsMusicDb() && item->HasMusicInfoTag())
		return item->GetMusicInfoTag()->GetURL();
	else
		return item->GetPath();
}
