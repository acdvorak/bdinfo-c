/* 
 * File:   parse_mpls.c
 * Author: Andrew C. Dvorak <andy@andydvorak.net>
 *
 * Created on January 21, 2013, 2:43 PM
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>


/*
 * Constants
 */


#define PLAYLIST_POS  8
#define CHAPTERS_POS 12
#define TIME_IN_POS  82
#define TIME_OUT_POS 86

#define CHAPTER_TYPE_ENTRY_MARK 1 /* standard chapter */
#define CHAPTER_TYPE_LINK_POINT 2 /* unsupported ??? */

#define CHAPTER_SIZE 14 /* number of bytes per chapter entry */

#define TIMECODE_DIV 45000.00 /* divide timecodes (int32) by this value to get
                                 the number of seconds (double) */


/*
 * Typedefs
 */





/*
 * Structs
 */


typedef struct {
    char filename[11]; /* uppercase - e.g., "12345.M2TS" */
    double time_in_sec;
    double time_out_sec;
    double duration_sec;
    double relative_time_in_sec;
    double relative_time_out_sec;
    int video_count;
    int audio_count;
    int subtitle_count; /* Presentation Graphics Streams (subtitles) */
    int interactive_menu_count; /* Interactive Graphics Streams (in-movie, on-screen, interactive menus) */
    int secondary_video_count;
    int secondary_audio_count;
    int pip_count; /* Picture-in-Picture (PiP) */
} stream_clip_t; /* .m2ts + .cpli */

typedef struct {
    char* filename; /* uppercase - e.g., "00801.MPLS" */
    double time_in_sec;
    double time_out_sec;
    double duration_sec;
    stream_clip_t* stream_clips;
    size_t stream_clip_count;
    double* chapters;
    size_t chapter_count;
} playlist_t;


/*
 * Function prototypes
 */


void
die(char* message);

long
file_get_length(FILE* file);

int16_t
get_int16(char* bytes);

int32_t
get_int32(char* bytes);

/**
 * Converts a sequence of bytes to an int16_t and advances the cursor position by +2.
 * @param bytes
 * @param cursor
 * @return 
 */
int16_t
get_int16_cursor(char* bytes, int* cursor);

/**
 * Converts a sequence of bytes to an int32_t and advances the cursor position by +4.
 * @param bytes
 * @param cursor
 * @return 
 */
int32_t
get_int32_cursor(char* bytes, int* cursor);

int16_t
file_read_int16(FILE* file, int offset);

int32_t
file_read_int32(FILE* file, int offset);

/**
 * Reads a sequence of bytes into an int16_t and advances the cursor position by +2.
 * @param file
 * @param offset
 * @return 
 */
int16_t
file_read_int16_cursor(FILE* file, int* offset);

/**
 * Reads a sequence of bytes into an int32_t and advances the cursor position by +4.
 * @param file
 * @param offset
 * @return 
 */
int32_t
file_read_int32_cursor(FILE* file, int* offset);

char*
file_read_string(FILE* file, int offset, int length);

/**
 * Reads a sequence of bytes into a C string and advances the cursor position by #{length}.
 * @param file
 * @param offset
 * @param length
 * @return 
 */
char*
file_read_string_cursor(FILE* file, int* offset, int length);

/**
 * Copies a sequence of bytes into a newly alloc'd C string and advances the cursor position by #{length}.
 * @param file
 * @param offset
 * @param length
 * @return 
 */
char*
copy_string_cursor(char* bytes, int* offset, int length);

/**
 * Converts the specified timecode to seconds.
 * @param timecode
 * @return Number of seconds (integral part) and milliseconds (fractional part) represented by the timecode
 */
double
timecode_in_sec(int32_t timecode);

/**
 * Converts a duration in seconds to a human-readable string in the format HH:MM:SS.mmm
 * @param length_sec
 * @return 
 */
char*
duration_human(double length_sec);

void
free_playlist_members(playlist_t* playlist)
{
    free(playlist->filename); playlist->filename = NULL;
    free(playlist->chapters); playlist->chapters = NULL;
}

void
parse_mpls(FILE* mplsFile);


/*
 * Implementation
 */


void
die(char* message)
{
    fprintf(stderr, "%s\n", message);
    exit(EXIT_FAILURE);
}

/**
 * Get the size of a file (in bytes).
 * @param file
 * @return Size of the file in bytes.
 */
long
file_get_length(FILE* file)
{
    // Beginning to End of file
    fseek(file, 0, SEEK_END);
    
    // Get file size
    long fSize = ftell(file);
    
    // Reset cursor to beginning of file
    rewind(file);
    
    return fSize;
}

int16_t
get_int16(char* bytes)
{
    uint8_t b1 = (uint8_t) bytes[0];
    uint8_t b2 = (uint8_t) bytes[1];
//    printf("%02x %02x \n", b1, b2);
    return (b1 << 8) |
           (b2 << 0);
}

int32_t
get_int32(char* bytes)
{
    uint8_t b1 = (uint8_t) bytes[0];
    uint8_t b2 = (uint8_t) bytes[1];
    uint8_t b3 = (uint8_t) bytes[2];
    uint8_t b4 = (uint8_t) bytes[3];
//    printf("%02x %02x %02x %02x \n", b1, b2, b3, b4);
    return (b1 << 24) |
           (b2 << 16) |
           (b3 <<  8) |
           (b4 <<  0);
}

int16_t
get_int16_cursor(char* bytes, int* cursor)
{
    int16_t val = get_int16(bytes + *cursor);
    *cursor += 2;
    return val;
}

int32_t
get_int32_cursor(char* bytes, int* cursor)
{
    int32_t val = get_int32(bytes + *cursor);
    *cursor += 4;
    return val;
}

int16_t
file_read_int16(FILE* file, int offset)
{
    char buff[3] = { 0, 0, 0 };
    
    fseek(file, offset, SEEK_SET);
    
    // Read 2 bytes of data into a char array
    // e.g., fread(chars, 1, 8) = first eight chars
    int br = fread(buff, 1, 2, file);
    
    if(br != 2)
    {
        die("Wrong number of chars read in file_read_int16()");
    }
    
    return get_int16(buff);
}

int32_t
file_read_int32(FILE* file, int offset)
{
    char buff[5] = { 0, 0, 0, 0, 0 };
    
    fseek(file, offset, SEEK_SET);
    
    // Read 4 bytes of data into a char array
    // e.g., fread(chars, 1, 8) = first eight chars
    int br = fread(buff, 1, 4, file);
    
    if(br != 4)
    {
        die("Wrong number of chars read in file_read_int32()");
    }
    
    return get_int32(buff);
}

int16_t
file_read_int16_cursor(FILE* file, int* offset)
{
    int16_t val = file_read_int16(file, *offset);
    *offset += 2;
    return val;
}

int32_t
file_read_int32_cursor(FILE* file, int* offset)
{
    int16_t val = file_read_int16(file, *offset);
    *offset += 4;
    return val;
}

char*
file_read_string(FILE* file, int offset, int length)
{
    // Allocate an empty string initialized to all NULL chars
    char* chars = (char*) calloc(length + 1, sizeof(char));
    
    // Jump to the requested position (byte offset) in the file
    fseek(file, offset, SEEK_SET);
    
    // Read 4 bytes of data into a char array
    // e.g., fread(chars, 1, 8) = first eight chars
    int br = fread(chars, 1, length, file);
    
    if(br != length)
    {
        die("Wrong number of chars read in file_get_chars()");
    }
    
    // Reset cursor to beginning of file
    rewind(file);

    return chars;
}

char*
file_read_string_cursor(FILE* file, int* offset, int length)
{
    char* str = file_read_string(file, *offset, length);
    *offset += length;
    return str;
}

char*
copy_string_cursor(char* bytes, int* offset, int length)
{
    char* str = (char*) calloc(length + 1, sizeof(char));
    strncpy(str, bytes + *offset, length);
    *offset += length;
    return str;
}

double
timecode_in_sec(int32_t timecode)
{
    return (double)timecode / TIMECODE_DIV;
}

char*
duration_human(double length_sec)
{
    char* str = (char*) calloc(15, sizeof(char));
    sprintf(str, "%02.0f:%02.0f:%06.3f", floor(length_sec / 3600), floor(fmod(length_sec, 3600) / 60), fmod(length_sec, 60));
    return str;
}

void
parse_mpls(FILE* mplsFile)
{
    // Verify file size
    long fileSize = file_get_length(mplsFile);
    if (fileSize < 90)
    {
        die("MPLS file is too small.");
    }
    
    int pos = 0;
    char* data = file_read_string(mplsFile, 0, fileSize);
    
    // Verify header
    char* header = copy_string_cursor(data, &pos, 8);
    if (strncmp("MPLS0100", header, 8) != 0 &&
        strncmp("MPLS0200", header, 8) != 0)
    {
        die("Invalid MPLS header: expected MPLS0100 or MPLS0200.");
    }
    free(header);
    header = NULL;
    
    // Verify playlist offset
    int32_t playlistPos = get_int32_cursor(data, &pos);
    if (playlistPos <= 8)
    {
        die("Invalid playlists offset.");
    }
    
    // Verify chapter offset
    int32_t chaptersPos = get_int32_cursor(data, &pos);
    if (chaptersPos <= 8)
    {
        die("Invalid chapters offset.");
    }
    
    int chapterCountPos = chaptersPos + 4;
    int chapterListPos = chaptersPos + 6;
    
    // Verify chapter count
    int16_t totalChapterCount = get_int16(data + chapterCountPos);
    if (totalChapterCount < 0)
    {
        die("Invalid chapter count.");
    }
    
    // Verify Time IN
    int32_t playlistTimeIn = get_int32(data + TIME_IN_POS);
    if (playlistTimeIn < 0)
    {
        die("Invalid playlist time in.");
    }
    
    // Verify Time OUT
    int32_t playlistTimeOut = get_int32(data + TIME_OUT_POS);
    if (playlistTimeOut < 0)
    {
        die("Invalid playlist time out.");
    }
    
    /*
     * Stream clips (.M2TS / .CLPI / .SSIF)
     */
    
    playlist_t playlist;
    
    pos = playlistPos;
    
    /*int32_t playlist_size = */ get_int32_cursor(data, &pos);
    /*int16_t playlist_reserved = */ get_int16_cursor(data, &pos);
    int16_t stream_clip_count = get_int16_cursor(data, &pos);
    /*int16_t playlist_subitem_count = */ get_int16_cursor(data, &pos);
    
    int streamClipIndex;
    
    double total_length_sec = 0;
    
    stream_clip_t* streamClips = (stream_clip_t*) calloc(stream_clip_count, sizeof(stream_clip_t));
    stream_clip_t* chapterStreamClips = (stream_clip_t*) calloc(stream_clip_count, sizeof(stream_clip_t));
    
    for(streamClipIndex = 0; streamClipIndex < stream_clip_count; streamClipIndex++)
    {
        stream_clip_t* streamClip = &streamClips[streamClipIndex];
        stream_clip_t* chapterStreamClip = &chapterStreamClips[streamClipIndex];

        int itemStart = pos;
        int itemLength = get_int16_cursor(data, &pos);
        char* itemName = copy_string_cursor(data, &pos, 5); /* e.g., "00504" */
        char* itemType = copy_string_cursor(data, &pos, 4); /* "M2TS" (or SSIF?) */
        
        // Will always be exactly ten (10) chars
        sprintf(streamClip->filename, "%s.%s", itemName, itemType);
        sprintf(chapterStreamClip->filename, "%s.%s", itemName, itemType);
        
        pos += 1;
        int multiangle = (data[pos] >> 4) & 0x01;
//        int condition  = (data[pos] >> 0) & 0x0F;
        pos += 2;

        int inTime = get_int32_cursor(data, &pos);
        if (inTime < 0) inTime &= 0x7FFFFFFF;
        double timeIn = (double)inTime / 45000;

        int outTime = get_int32_cursor(data, &pos);
        if (outTime < 0) outTime &= 0x7FFFFFFF;
        double timeOut = (double)outTime / 45000;

        streamClip->time_in_sec = timeIn;
        streamClip->time_out_sec = timeOut;
        streamClip->duration_sec = timeOut - timeIn;
        streamClip->relative_time_in_sec = total_length_sec;
        streamClip->relative_time_out_sec = streamClip->relative_time_in_sec + streamClip->duration_sec;
        
#ifdef DEBUG
        printf("time in: %8.3f.  time out: %8.3f.  duration: %8.3f.  relative time in: %8.3f.\n",
                streamClip->time_in_sec, streamClip->time_out_sec,
                streamClip->duration_sec, streamClip->relative_time_in_sec);
#endif
        
        // Copy values
        chapterStreamClip->time_in_sec = streamClip->time_in_sec;
        chapterStreamClip->time_out_sec = streamClip->time_out_sec;
        chapterStreamClip->duration_sec = streamClip->duration_sec;
        chapterStreamClip->relative_time_in_sec = streamClip->relative_time_in_sec;
        chapterStreamClip->relative_time_out_sec = streamClip->relative_time_out_sec;
        
        total_length_sec += (timeOut - timeIn);
        
#ifdef DEBUG
        printf("Stream clip %2i: %s (type = %s, length = %i, multiangle = %i)\n", streamClipIndex, streamClip->filename, itemType, itemLength, multiangle);
#endif
        
        free(itemName); itemName = NULL;
        free(itemType); itemType = NULL;
        
//        StreamClips.Add(streamClip);
//        chapterClips.Add(streamClip);

        pos += 12;
        
        if (multiangle > 0)
        {
            int angles = data[pos];
            pos += 2;
            int angle;
            for (angle = 0; angle < angles - 1; angle++)
            {
                /* char* angleName = */ copy_string_cursor(data, &pos, 5);
                /* char* angleType = */ copy_string_cursor(data, &pos, 4);
                pos += 1;

                // TODO
                /*
                TSStreamFile angleFile = null;
                string angleFileName = string.Format(
                    "{0}.M2TS", angleName);
                if (streamFiles.ContainsKey(angleFileName))
                {
                    angleFile = streamFiles[angleFileName];
                }
                if (angleFile == null)
                {
                    throw new Exception(string.Format(
                        "Playlist {0} referenced missing angle file {1}.",
                        _fileInfo.Name, angleFileName));
                }

                TSStreamClipFile angleClipFile = null;
                string angleClipFileName = string.Format(
                    "{0}.CLPI", angleName);
                if (streamClipFiles.ContainsKey(angleClipFileName))
                {
                    angleClipFile = streamClipFiles[angleClipFileName];
                }
                if (angleClipFile == null)
                {
                    throw new Exception(string.Format(
                        "Playlist {0} referenced missing angle file {1}.",
                        _fileInfo.Name, angleClipFileName));
                }

                TSStreamClip angleClip =
                    new TSStreamClip(angleFile, angleClipFile);
                angleClip.AngleIndex = angle + 1;
                angleClip.TimeIn = streamClip.TimeIn;
                angleClip.TimeOut = streamClip.TimeOut;
                angleClip.RelativeTimeIn = streamClip.RelativeTimeIn;
                angleClip.RelativeTimeOut = streamClip.RelativeTimeOut;
                angleClip.Length = streamClip.Length;
                StreamClips.Add(angleClip);
                */
            }
            // TODO
//            if (angles - 1 > AngleCount) AngleCount = angles - 1;
        }

        /* int streamInfoLength = */ get_int16_cursor(data, &pos);
        pos += 2;
        int streamCountVideo = data[pos++];
        int streamCountAudio = data[pos++];
        int streamCountPG = data[pos++];
        int streamCountIG = data[pos++];
        int streamCountSecondaryAudio = data[pos++];
        int streamCountSecondaryVideo = data[pos++];
        int streamCountPIP = data[pos++];
        pos += 5;
        
        int i;

        for (i = 0; i < streamCountVideo; i++)
        {
//            TSStream stream = CreatePlaylistStream(data, ref pos);
//            if (stream != null) PlaylistStreams[stream.PID] = stream;
        }
        for (i = 0; i < streamCountAudio; i++)
        {
//            TSStream stream = CreatePlaylistStream(data, ref pos);
//            if (stream != null) PlaylistStreams[stream.PID] = stream;
        }
        for (i = 0; i < streamCountPG; i++)
        {
//            TSStream stream = CreatePlaylistStream(data, ref pos);
//            if (stream != null) PlaylistStreams[stream.PID] = stream;
        }
        for (i = 0; i < streamCountIG; i++)
        {
//            TSStream stream = CreatePlaylistStream(data, ref pos);
//            if (stream != null) PlaylistStreams[stream.PID] = stream;
        }
        for (i = 0; i < streamCountSecondaryAudio; i++)
        {
//            TSStream stream = CreatePlaylistStream(data, ref pos);
//            if (stream != null) PlaylistStreams[stream.PID] = stream;
            pos += 2;
        }
        for (i = 0; i < streamCountSecondaryVideo; i++)
        {
//            TSStream stream = CreatePlaylistStream(data, ref pos);
//            if (stream != null) PlaylistStreams[stream.PID] = stream;
            pos += 6;
        }
        /*
         * TODO
         * 
        for (i = 0; i < streamCountPIP; i++)
        {
            TSStream stream = CreatePlaylistStream(data, ref pos);
            if (stream != null) PlaylistStreams[stream.PID] = stream;
        }
        */

        pos += itemLength - (pos - itemStart) + 2;
        
        streamClip->video_count = streamCountVideo;
        streamClip->audio_count = streamCountAudio;
        streamClip->subtitle_count = streamCountPG;
        streamClip->interactive_menu_count = streamCountIG;
        streamClip->secondary_video_count = streamCountSecondaryVideo;
        streamClip->secondary_audio_count = streamCountSecondaryAudio;
        streamClip->pip_count = streamCountPIP;
        
        chapterStreamClip->video_count = streamCountVideo;
        chapterStreamClip->audio_count = streamCountAudio;
        chapterStreamClip->subtitle_count = streamCountPG;
        chapterStreamClip->interactive_menu_count = streamCountIG;
        chapterStreamClip->secondary_video_count = streamCountSecondaryVideo;
        chapterStreamClip->secondary_audio_count = streamCountSecondaryAudio;
        chapterStreamClip->pip_count = streamCountPIP;
        
#ifdef DEBUG
        printf("\t\t #V: %i, #A: %i, #PG: %i, #IG: %i, #2A: %i, #2V: %i, #PiP: %i \n",
                streamCountVideo, streamCountAudio, streamCountPG, streamCountIG,
                streamCountSecondaryAudio, streamCountSecondaryVideo, streamCountPIP);
#endif
    }

    /*
     * Chapters
     */
    
    pos = chapterListPos;
    
    int i;
    int validChapterCount = 0;
    
    for (i = 0; i < totalChapterCount; i++)
    {
        char* chapter = data + pos + (i * CHAPTER_SIZE);
        if (chapter[1] == CHAPTER_TYPE_ENTRY_MARK)
        {
            validChapterCount++;
        }
    }
    
    double* chapters = (double*) calloc(validChapterCount, sizeof(double));
    
    validChapterCount = 0;
    
    pos = chapterListPos;
    
    for (i = 0; i < totalChapterCount; i++)
    {
        char* chapter = data + pos;
        
        if (chapter[1] == CHAPTER_TYPE_ENTRY_MARK)
        {
            
            int streamFileIndex = ((int)data[pos + 2] << 8) + data[pos + 3];
            
//            printf("streamFileIndex = %i\n", streamFileIndex);
            
            int32_t chapterTime = get_int32(chapter + 4);

//            int64_t chapterTime =
//                ((int64_t)data[pos + 4] << 24) +
//                ((int64_t)data[pos + 5] << 16) +
//                ((int64_t)data[pos + 6] <<  8) +
//                ((int64_t)data[pos + 7]);

            stream_clip_t* streamClip = &chapterStreamClips[streamFileIndex];
        
//            printf("\t\t #V: %i, #A: %i, #PG: %i, #IG: %i, #2A: %i, #2V: %i, #PiP: %i \n",
//                    streamClip->video_count, streamClip->audio_count,
//                    streamClip->subtitle_count, streamClip->interactive_menu_count,
//                    streamClip->secondary_audio_count, streamClip->secondary_video_count,
//                    streamClip->pip_count);

            double chapterSeconds = (double)chapterTime / 45000;

            double relativeSeconds =
                chapterSeconds -
                streamClip->time_in_sec +
                streamClip->relative_time_in_sec;
            
#ifdef DEBUG
            printf("streamFileIndex %2i: (%9i / 45000 = %8.3f) - %8.3f + %8.3f = %8.3f\n", streamFileIndex, chapterTime, chapterSeconds, streamClip->time_in_sec, streamClip->relative_time_in_sec, relativeSeconds);
#endif

            // Ignore short last chapter
            // If the last chapter is < 1.0 Second before end of film Ignore
            if (total_length_sec - relativeSeconds > 1.0)
            {
//                streamClip->Chapters.Add(chapterSeconds);
//                this.Chapters.Add(relativeSeconds);
                chapters[validChapterCount++] = relativeSeconds;
            }
        }
        
        pos += CHAPTER_SIZE;
    }
    
    chapters = (double*) realloc(chapters, validChapterCount * sizeof(double));

    playlist.time_in_sec = playlistTimeIn;
    playlist.time_out_sec = playlistTimeOut;
    playlist.duration_sec = total_length_sec;
    playlist.stream_clips = streamClips;
    playlist.stream_clip_count = stream_clip_count;
    playlist.chapters = chapters;
    playlist.chapter_count = validChapterCount;
    
    char* playlist_duration_human = duration_human(playlist.duration_sec);

    printf("Playlist length: %s\n", playlist_duration_human);
    printf("Chapter count: %i\n", validChapterCount);
    
    free(playlist_duration_human);
    
    int c;
    for(c = 0; c < playlist.chapter_count; c++)
    {
        double sec = playlist.chapters[c];
        char* chapter_start_human = duration_human(sec);
        printf("Chapter %2i: %s\n", c + 1, chapter_start_human);
        free(chapter_start_human);
    }
    
    free(streamClips);
    free(chapterStreamClips);
    free_playlist_members(&playlist);
    free(data);
}


/*
 * 
 */
int main(int argc, char** argv) {
    if (argc < 2)
    {
        die("Usage: parse_mpls MPLS_FILE_PATH [ MPLS_FILE_PATH ... ]");
    }
    
    int i;
    for(i = 1; i < argc; i++)
    {
        FILE* mplsFile = fopen(argv[1], "r");

        if (mplsFile == NULL)
        {
            die("Unable to open MPLS file.");
        }

        parse_mpls(mplsFile);
    }
    
    return (EXIT_SUCCESS);
}

