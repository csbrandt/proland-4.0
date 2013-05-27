class MyProducer : public TileProducer
{

public:
    MyProducer(Ptr<TileCache> cache, Ptr<TileProducer> input, ...) :
    	TileProducer("MyProducer", "MyCreateTile")
    {
		init(cache, input, ...);
	}

    virtual ~MyProducer()
    {
	}


protected:
    MyProducer() : TileProducer("MyProducer", "MyCreateTile")
    {
	}

    void init(Ptr<TileCache> cache, Ptr<TileProducer> input, ...)
    {
	    TileProducer::init(cache, true);
    	this->input = input;
    	...
	}

    virtual Ptr<Task> startCreateTile(int level, int tx, int ty,
        unsigned int deadline, Ptr<Task> task, Ptr<TaskGraph> owner)
    {
		Ptr<TaskGraph> result = owner == NULL ? new TaskGraph(task) : owner;
		TileCache::Tile *t = input->getTile(level, tx, ty, deadline);
		result->addTask(t->task);
		result->addDependency(task, t->task);
		return result;
	}

    virtual bool doCreateTile(int level, int tx, int ty, TileStorage::Slot *data)
    {
    	CPUTileStorage<unsigned char>::CPUSlot *in;
    	GPUTileStorage::GPUSlot *out;
        TileCache::Tile *t = intput->findTile(level, tx, ty);
        in = dynamic_cast<CPUTileStorage<unsigned char>::CPUSlot*>(t->getData());
    	out = dynamic_cast<GPUTileStorage::GPUSlot*>(data);
        ...
    	getCache()->getStorage().cast<GPUTileStorage>()->notifyChange(out);
	}

    virtual void stopCreateTile(int level, int tx, int ty)
    {
        TileCache::Tile *t = input->findTile(level, tx, ty);
        input->putTile(t);
	}

private:
    Ptr<TileProducer> input;
};
