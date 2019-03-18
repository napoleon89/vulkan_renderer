struct Array {
	u8 *data;
	int count;
	int reserve_count;
	int block_size;
	
	void reserveSpace(u32 count, u32 new_block_size) {
		if(data != 0) Platform::free(data);
		block_size = new_block_size;
		data = Platform::alloc(count * block_size);
		reserve_count = count;
		count = 0;
	}
	
	u32 addItem(void *item) {
		Assert(count < reserve_count);
		u8 *walker = data + count * block_size;	
		
	}
};