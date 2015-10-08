
/* Prints the value to the log file with #awesomesauce
   Author: Alessio
*/
void print_value_to_file(unsigned char c)
{
	int a,b;
	unsigned char uns = (unsigned char) c;

   //even: read most significant bytes
	 if(value_counter % 2 == 0) {
		 value_to_print = uns;
		 value_to_print = (value_to_print << 8) & 0xFF00;

		 //a = (int) c;
		 //fprintf(log_file,"%i ",a);
	 }

  //odd: read least signifative bytes and print the value
	 else {
		 value_to_print |= uns;

		fprintf(log_file,"%hu ",value_to_print);
		value_to_print = 0;

		//b = (int) c;
    //	fprintf(log_file,"%i ",b);
		// if(value_counter % 14 == 1) fprintf(log_file, ": ");
	 }

	 value_counter++;
}
