extern "C" {

__declspec(dllexport) int sample_proc()
{
	return 123;
}

__declspec(dllexport) int sample_data = 123;

}