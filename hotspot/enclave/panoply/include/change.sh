for file in $(ls sgx_*.h); do
	cp $file ../cpp/"${file%.h}.cpp"
done
