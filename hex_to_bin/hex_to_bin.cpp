// hex_to_bin.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <Windows.h>
#include <stdio.h>
#include <math.h>

#define MAX_HEX_FILE_SIZE 1000000

void update_bin_array(
    char *bin_array, 
    int total_byte_count, //index to put the byte formed from temp_byte and temp_byte2
    char temp_byte, 
    char temp_byte_two);

// print some stats info on the binary file
void do_stats(char *bin_array, int total_byte_count);


int main(int argc, char** argv)
{
    if (argc != 2) {
        printf("Usage: $ %s <HEX-FILE>", argv[0]);
        return 0;
    }

    ///////////////////////////////
    // Read the input HEX file
    HANDLE h_file = CreateFileA(argv[1], GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h_file == INVALID_HANDLE_VALUE) {
        printf("Couldn't open the file %s for reading... quitting...\n", argv[1]);
        return -1;
    }

    DWORD file_size_high = 0;
    DWORD file_size = GetFileSize(h_file, &file_size_high);
    
    if (file_size_high != 0 || file_size>MAX_HEX_FILE_SIZE) {
        printf("Sorry, that HEX file is too big... quitting...\n");
        return -2;
    }

    // Hold the file bytes of the (ascii text) intel hex file in here:
    char* file_bytes = (char*)malloc(file_size);
    if (!file_bytes) {
        printf("Couldn't allocate %d bytes to read the file... quitting...\n", file_size);
        return -3;
    }

    DWORD num_red = 0;
    BOOL did_read = ReadFile(h_file, file_bytes, file_size, &num_red, NULL);
    if (!did_read) {
        printf("File read failed... quitting...\n");
        return -4;
    }
    if (num_red != file_size) {
        printf("File read failed... quitting...\n");
        return -5;
    }
    ////
    /////////////////////////////////


    ////////////////////////////////
    // Process the Intel HEX to create a raw binary file 
    // (hold the bin in the following array):
    char* bin_array = (char*)malloc(file_size / 2); // the true array soze is necessarily smaller than file_size/2, so this will suffice 

    int char_in_line_counter = 0;
    int total_byte_count = 0;
    for (int i = 0; i < file_size; i++) {
        char temp_byte = file_bytes[i]; // HEX file byte to be processed in current loop iteration
        
        //if we hit a new line reset line char counter and continue
        if (temp_byte == '\r' || temp_byte == '\n') { // blerg... on WINDOWS we hit '\r' first and then '\n' not just '\n'
            char_in_line_counter = 0;
            continue;
        }

        // HEX format uses columns 0...8 for address into, not binary data, and uses columns 41 and 42 as a checksum
        if (char_in_line_counter > 8 && char_in_line_counter <=40) {
            // we are in the "data" part so convert to bin from hex 
            char temp_byte_two = file_bytes[i + 1];
            i++;
            char_in_line_counter++;
            char_in_line_counter++; // inc twice since we grabbed two chars to convert to one byte, whereas 'i' will auto-increment once in loop
            
            //convert the two ascii characters to a byte and pop it into the bin_array at location total_byte_count
            update_bin_array(bin_array, total_byte_count, temp_byte, temp_byte_two);
            total_byte_count++;
            continue;
        }
        else { 
            // we are still in the addressing part or we are in the checksum part, we just ignore both
            char_in_line_counter++;
            continue;
        }


    }

    if(file_bytes)free(file_bytes);
    if(h_file)CloseHandle(h_file);

    // Uncomment the below function if you want some stats
    //do_stats(bin_array, total_byte_count);

    // Output the BIN file to a file name output.bin (if it doesn't aleady exist):
    HANDLE h_out = CreateFileA("output.bin", GENERIC_WRITE, 0, NULL, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h_out == INVALID_HANDLE_VALUE) {
        printf("Couldn't open output file for writing... does it already exist? Quitting...\n");
        return -6;
    }

    DWORD num_written = 0;
    BOOL did_write = WriteFile(h_out, bin_array, total_byte_count, &num_written, NULL);
    if (!did_write) {
        printf("Couldn't write to output file... quitting...\n");
        return -7;
    }
    if (num_written != total_byte_count) {
        printf("Couldn't write to output file... quitting...\n");
        return -7;
    }

    if(h_out)CloseHandle(h_out);
    return 0;
}

void update_bin_array(char* bin_array,int total_byte_count, //index to put the byte formed from temp_byte and temp_byte2
    char temp_byte, char temp_byte_two) {

    char temp_ptr[2];
    temp_ptr[0] = temp_byte;
    temp_ptr[1] = temp_byte_two;
    unsigned char bin_byte_val = 0;
    //sscanf(temp_ptr, "%2hhx", &bin_byte_val);
    sscanf_s(temp_ptr, "%2hhx", &bin_byte_val);
    bin_array[total_byte_count] = bin_byte_val;
    return;
}

void do_stats(char* bin_array, int total_byte_count) {

    // Get a histogram of the byte counts in the bin_array
    int hist_array[256];
    double prob_array[256]; // another array to hold probabilities for the entropy calculation

    // zero arrays
    for (int i = 0; i < 256; i++) {
        hist_array[i] = 0;
        prob_array[i] = 0;
    }

    // get histogram array:
    for (int i = 0; i < total_byte_count; i++) {
        unsigned char b_index = bin_array[i];
        int int_index = (int)b_index;
        hist_array[int_index]++;
    }

    // Print a historgram of the byte counts
    for (int i = 0; i < 256; i++) {
        printf("%d (%x), %d\n", i, i, hist_array[i]);
    }

    // Also calculate the file entropy relative to the max entropy
    double max_ent = log(256); // this is actually max_ent/k, where k is some constant that cancels out in the ratio.
    double ent_acc = 0; //accumulate the value proportional to the file bytes entropy
    for (int i = 0; i < 256; i++) {
        double dbl_temp = (double)hist_array[i];
        double prob_temp = dbl_temp / total_byte_count;
        double ent_temp = prob_temp * log(prob_temp);
        ent_acc += ent_temp;
    }
    double ent_ratio = -ent_acc / max_ent; // This value is always less than 1 or equal to
    
    // Also print the file entropy as a percentage
    printf("BIN file entropy: %f%%\n", 100.0 * ent_ratio);
}
