/********************************************************************
	created:	2018年12月3日13:45:51
	filename: 	SingleQueueStd.h
	author:		yuwenjin
	
	purpose:	与SingleQueue相同，只不过使用标准的shared_ptr
*********************************************************************/

#include <memory>
using namespace std;

template<typename T>
class SingleQueueStd
{
public:
    typedef shared_ptr<T> DataRef;

private:
	SingleQueueStd<T>& operator =(const SingleQueueStd<T>&) {}

    struct DataPlus;
    typedef shared_ptr<DataPlus>   DataPlusRef;
    struct DataPlus
    {
        DataRef         data;
        bool            hasRead;
        volatile bool   hasNext;
        DataPlusRef     next;
        DataPlus( DataRef dataRef ) : data(dataRef), hasRead(false), hasNext(false) {}
        DataPlus() : hasRead(true), hasNext(false) {}
    };
    DataPlusRef         m_write;
    DataPlusRef         m_read;
    unsigned int        m_writeCount;
    unsigned int        m_readCount;
    const unsigned int  m_maxSize;

public:
	SingleQueueStd(unsigned int queueLimit = 30000) : m_maxSize( queueLimit )
    {
        m_writeCount = m_readCount = 0;
        m_read.reset(new DataPlus());
        m_write = m_read;
    }
	SingleQueueStd(SingleQueueStd& que) : m_maxSize(que.m_maxSize)
    {
        m_writeCount    = que.m_writeCount;
        m_readCount     = que.m_readCount;
        m_read.swap( que.m_read );
        m_write.swap( que.m_write );
        que.m_readCount = que.m_writeCount;
    }

    bool Write( DataRef dataRef )
    {
        if ( Size()>m_maxSize )
            return false;

        DataPlusRef plusRef( new DataPlus(dataRef) );
        if ( !plusRef )
        {
            throw "DataPlus::Write: new DataPlus false";
        }

        volatile bool& flag = m_write->hasNext;
        m_write->next       = plusRef;
        m_write             = plusRef;  // release
        ++m_writeCount;
        flag                = true;     // keyword
        return true;
    }

    DataRef Read()
    {
        if ( !m_read->hasRead )
        {
            m_read->hasRead = true;
            ++m_readCount;
            return m_read->data;
        }

        if ( m_read->hasNext )      // keyword
        {
            m_read = m_read->next;  // release
            return Read();
        }

        return DataRef();
    }

    unsigned int Size() { return (m_writeCount-m_readCount); }
};