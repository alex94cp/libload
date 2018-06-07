extern "C" {

int sample_data = 123;

int sample_proc()
{
	return 123;
}

#if defined(_MSC_VER) || defined(__DMC__)

int sample_seh()
{
	__try {
		*static_cast<int *>(nullptr) = 0;
	} __finally {
		return 123;
	}
}

#endif

}
