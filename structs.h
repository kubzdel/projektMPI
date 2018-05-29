struct sectionRequest{
    int clock;
    int section;
	int id;
	bool operator <(const sectionRequest & other) const
	{
		return (clock == other.clock ? id < other.id : clock < other.clock );
	}
};

struct sectionResponse{
    int status;
    int section;
};
