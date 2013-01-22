/* 
 * File:   parse_mpls.c
 * Author: Andrew C. Dvorak <andy@andydvorak.net>
 *
 * Created on January 21, 2013, 2:43 PM
 */


#include "parse_mpls.h"


/*
 * Utility functions
 */


void
die (const char* filename, int line_number, const char * format, ...)
{
    va_list vargs;
    va_start (vargs, format);
    fprintf (stderr, "%s:%d: ", filename, line_number);
    vfprintf (stderr, format, vargs);
    fprintf (stderr, "\n");
    exit (EXIT_FAILURE);
}

double
timecode_to_sec(int32_t timecode)
{
    return (double)timecode / TIMECODE_DIV;
}

char*
format_duration(double length_sec)
{
    char* str = (char*) calloc(15, sizeof(char));
    sprintf(str, "%02.0f:%02.0f:%06.3f", floor(length_sec / 3600), floor(fmod(length_sec, 3600) / 60), fmod(length_sec, 60));
    return str;
}


/*
 * Binary file handling
 */


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
        DIE("Wrong number of chars read in file_get_chars(): expected %i, found %i", length, br);
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


/*
 * Struct initialization
 */


mpls_file_t
create_mpls_file_t()
{
    mpls_file_t mpls_file;
    init_mpls_file_t(&mpls_file);
    return mpls_file;
}

void
init_mpls_file_t(mpls_file_t* mpls_file)
{
    int i;
    mpls_file->path = NULL;
    mpls_file->name = NULL;
    mpls_file->file = NULL;
    mpls_file->size = 0;
    mpls_file->data = NULL;
    for (i = 0; i < 9; i++)
        mpls_file->header[i] = 0;
    mpls_file->pos = 0;
    mpls_file->playlist_pos = 0;
    mpls_file->chapter_pos = 0;
    mpls_file->total_chapter_count = 0;
    mpls_file->time_in = 0;
    mpls_file->time_out = 0;
}

stream_clip_t
create_stream_clip_t()
{
    stream_clip_t stream_clip;
    init_stream_clip_t(&stream_clip);
    return stream_clip;
}

void
init_stream_clip_t(stream_clip_t* stream_clip)
{
    int i;
    for (i = 0; i < 11; i++)
        stream_clip->filename[i] = 0;
    stream_clip->time_in_sec = 0;
    stream_clip->time_out_sec = 0;
    stream_clip->duration_sec = 0;
    stream_clip->relative_time_in_sec = 0;
    stream_clip->relative_time_out_sec = 0;
    stream_clip->video_count = 0;
    stream_clip->audio_count = 0;
    stream_clip->subtitle_count = 0;
    stream_clip->interactive_menu_count = 0;
    stream_clip->secondary_video_count = 0;
    stream_clip->secondary_audio_count = 0;
    stream_clip->pip_count = 0;
}

playlist_t
create_playlist_t()
{
    playlist_t playlist;
    init_playlist_t(&playlist);
    return playlist;
}

void
init_playlist_t(playlist_t* playlist)
{
    int i;
    for (i = 0; i < 11; i++)
        playlist->filename[i] = 0;
    playlist->time_in_sec = 0;
    playlist->time_out_sec = 0;
    playlist->duration_sec = 0;
    playlist->stream_clips = NULL;
    playlist->stream_clip_count = 0;
    playlist->chapters = NULL;
    playlist->chapter_count = 0;
}


/*
 * Struct member copying
 */


void
copy_stream_clip(stream_clip_t* src, stream_clip_t* dest)
{
    strncpy(dest->filename, src->filename, 11);
    dest->time_in_sec = src->time_in_sec;
    dest->time_out_sec = src->time_out_sec;
    dest->duration_sec = src->duration_sec;
    dest->relative_time_in_sec = src->relative_time_in_sec;
    dest->relative_time_out_sec = src->relative_time_out_sec;
    dest->video_count = src->video_count;
    dest->audio_count = src->audio_count;
    dest->subtitle_count = src->subtitle_count;
    dest->interactive_menu_count = src->interactive_menu_count;
    dest->secondary_video_count = src->secondary_video_count;
    dest->secondary_audio_count = src->secondary_audio_count;
    dest->pip_count = src->pip_count;
}


/*
 * Struct freeing
 */


void
free_mpls_file_members(mpls_file_t* mpls_file)
{
    free(mpls_file->path); mpls_file->path = NULL;
    free(mpls_file->data); mpls_file->data = NULL;
}

void
free_playlist_members(playlist_t* playlist)
{
    free(playlist->chapters); playlist->chapters = NULL;
}


/*
 * Private inline functions
 */


static void
copy_header(char* header, char* data, int* offset)
{
    strncpy(header, data + *offset, 8);
    *offset += 8;
}


/*
 * Main parsing functions
 */


mpls_file_t
init_mpls(char* path)
{
    mpls_file_t mpls_file = create_mpls_file_t();

    mpls_file.path = realpath(path, NULL);
    if (mpls_file.path == NULL)
    {
        DIE("Unable to get the full path (realpath) of \"%s\".", path);
    }
    
    mpls_file.name = basename(mpls_file.path);
    if (mpls_file.name == NULL)
    {
        DIE("Unable to get the file name (basename) of \"%s\".", path);
    }
    
    mpls_file.file = fopen(mpls_file.path, "r");
    if (mpls_file.file == NULL)
    {
        DIE("Unable to open \"%s\" for reading.", mpls_file.path);
    }

    mpls_file.size = file_get_length(mpls_file.file);
    if (mpls_file.size < 90)
    {
        DIE("Invalid MPLS file (too small): \"%s\".", mpls_file.path);
    }
    
    mpls_file.data = file_read_string(mpls_file.file, 0, mpls_file.size);
    
    char* data = mpls_file.data;
    int* pos_ptr = &(mpls_file.pos);
    
    // Verify header
    copy_header(mpls_file.header, data, pos_ptr);
    if (strncmp("MPLS0100", mpls_file.header, 8) != 0 &&
        strncmp("MPLS0200", mpls_file.header, 8) != 0)
    {
        DIE("Invalid header in \"%s\": expected MPLS0100 or MPLS0200, found \"%s\".", mpls_file.path, mpls_file.header);
    }
    
    // Verify playlist offset
    mpls_file.playlist_pos = get_int32_cursor(data, pos_ptr);
    if (mpls_file.playlist_pos <= 8)
    {
        DIE("Invalid playlists offset: %i.", mpls_file.playlist_pos);
    }
    
    // Verify chapter offset
    int32_t chaptersPos = get_int32_cursor(data, pos_ptr);
    if (chaptersPos <= 8)
    {
        DIE("Invalid chapters offset: %i.", chaptersPos);
    }
    
    int chapterCountPos = chaptersPos + 4;
    mpls_file.chapter_pos = chaptersPos + 6;
    
    // Verify chapter count
    mpls_file.total_chapter_count = get_int16(data + chapterCountPos);
    if (mpls_file.total_chapter_count < 0)
    {
        DIE("Invalid chapter count: %i.", mpls_file.total_chapter_count);
    }
    
    // Verify Time IN
    mpls_file.time_in = get_int32(data + TIME_IN_POS);
    if (mpls_file.time_in < 0)
    {
        DIE("Invalid playlist time in: %i.", mpls_file.time_in);
    }
    
    // Verify Time OUT
    mpls_file.time_out = get_int32(data + TIME_OUT_POS);
    if (mpls_file.time_out < 0)
    {
        DIE("Invalid playlist time out: %i.", mpls_file.time_out);
    }
    
    return mpls_file;
}

void
parse_mpls(char* path)
{
    mpls_file_t mpls_file = init_mpls(path);
    
    int* pos_ptr = &(mpls_file.pos);
    char* data = mpls_file.data;
    
    /*
     * Stream clips (.M2TS / .CLPI / .SSIF)
     */
    
    playlist_t playlist = create_playlist_t();
    
    *pos_ptr = mpls_file.playlist_pos;
    
    /*int32_t playlist_size = */ get_int32_cursor(data, pos_ptr);
    /*int16_t playlist_reserved = */ get_int16_cursor(data, pos_ptr);
    int16_t stream_clip_count = get_int16_cursor(data, pos_ptr);
    /*int16_t playlist_subitem_count = */ get_int16_cursor(data, pos_ptr);
    
    int streamClipIndex;
    
    double total_length_sec = 0;
    
    stream_clip_t* streamClips = (stream_clip_t*) calloc(stream_clip_count, sizeof(stream_clip_t));
    stream_clip_t* chapterStreamClips = (stream_clip_t*) calloc(stream_clip_count, sizeof(stream_clip_t));
    
    for(streamClipIndex = 0; streamClipIndex < stream_clip_count; streamClipIndex++)
    {
        stream_clip_t* streamClip = &streamClips[streamClipIndex];
        stream_clip_t* chapterStreamClip = &chapterStreamClips[streamClipIndex];
        
        init_stream_clip_t(streamClip);

        int itemStart = *pos_ptr;
        int itemLength = get_int16_cursor(data, pos_ptr);
        char* itemName = copy_string_cursor(data, pos_ptr, 5); /* e.g., "00504" */
        char* itemType = copy_string_cursor(data, pos_ptr, 4); /* "M2TS" (or SSIF?) */
        
        // Will always be exactly ten (10) chars
        sprintf(streamClip->filename, "%s.%s", itemName, itemType);
        
        *pos_ptr += 1;
        int multiangle = (data[*pos_ptr] >> 4) & 0x01;
//        int condition  = (data[pos] >> 0) & 0x0F;
        *pos_ptr += 2;

        int32_t inTime = get_int32_cursor(data, pos_ptr);
        if (inTime < 0) inTime &= 0x7FFFFFFF;
        double timeIn = timecode_to_sec(inTime);

        int32_t outTime = get_int32_cursor(data, pos_ptr);
        if (outTime < 0) outTime &= 0x7FFFFFFF;
        double timeOut = timecode_to_sec(outTime);

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
        
        total_length_sec += (timeOut - timeIn);
        
#ifdef DEBUG
        printf("Stream clip %2i: %s (type = %s, length = %i, multiangle = %i)\n", streamClipIndex, streamClip->filename, itemType, itemLength, multiangle);
#endif
        
        free(itemName); itemName = NULL;
        free(itemType); itemType = NULL;

        *pos_ptr += 12;
        
        if (multiangle > 0)
        {
            int angles = data[*pos_ptr];
            *pos_ptr += 2;
            int angle;
            for (angle = 0; angle < angles - 1; angle++)
            {
                /* char* angleName = */ copy_string_cursor(data, pos_ptr, 5);
                /* char* angleType = */ copy_string_cursor(data, pos_ptr, 4);
                *pos_ptr += 1;

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

        /* int streamInfoLength = */ get_int16_cursor(data, pos_ptr);
        *pos_ptr += 2;
        int streamCountVideo = data[(*pos_ptr)++];
        int streamCountAudio = data[(*pos_ptr)++];
        int streamCountPG = data[(*pos_ptr)++];
        int streamCountIG = data[(*pos_ptr)++];
        int streamCountSecondaryAudio = data[(*pos_ptr)++];
        int streamCountSecondaryVideo = data[(*pos_ptr)++];
        int streamCountPIP = data[(*pos_ptr)++];
        *pos_ptr += 5;
        
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
            *pos_ptr += 2;
        }
        for (i = 0; i < streamCountSecondaryVideo; i++)
        {
//            TSStream stream = CreatePlaylistStream(data, ref pos);
//            if (stream != null) PlaylistStreams[stream.PID] = stream;
            *pos_ptr += 6;
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

        *pos_ptr += itemLength - (*pos_ptr - itemStart) + 2;
        
        streamClip->video_count = streamCountVideo;
        streamClip->audio_count = streamCountAudio;
        streamClip->subtitle_count = streamCountPG;
        streamClip->interactive_menu_count = streamCountIG;
        streamClip->secondary_video_count = streamCountSecondaryVideo;
        streamClip->secondary_audio_count = streamCountSecondaryAudio;
        streamClip->pip_count = streamCountPIP;
        
        copy_stream_clip(streamClip, chapterStreamClip);
        
#ifdef DEBUG
        printf("\t\t #V: %i, #A: %i, #PG: %i, #IG: %i, #2A: %i, #2V: %i, #PiP: %i \n",
                streamCountVideo, streamCountAudio, streamCountPG, streamCountIG,
                streamCountSecondaryAudio, streamCountSecondaryVideo, streamCountPIP);
#endif
    }

    /*
     * Chapters
     */
    
    *pos_ptr = mpls_file.chapter_pos;
    
    int i;
    int validChapterCount = 0;
    
    for (i = 0; i < mpls_file.total_chapter_count; i++)
    {
        char* chapter = data + *pos_ptr + (i * CHAPTER_SIZE);
        if (chapter[1] == CHAPTER_TYPE_ENTRY_MARK)
        {
            validChapterCount++;
        }
    }
    
    double* chapters = (double*) calloc(validChapterCount, sizeof(double));
    
    validChapterCount = 0;
    
    *pos_ptr = mpls_file.chapter_pos;
    
    for (i = 0; i < mpls_file.total_chapter_count; i++)
    {
        char* chapter = data + *pos_ptr;
        
        if (chapter[1] == CHAPTER_TYPE_ENTRY_MARK)
        {
            
            int streamFileIndex = ((int)data[*pos_ptr + 2] << 8) + data[*pos_ptr + 3];
            
            int32_t chapterTime = get_int32(chapter + 4);

            stream_clip_t* streamClip = &chapterStreamClips[streamFileIndex];

            double chapterSeconds = timecode_to_sec(chapterTime);

            double relativeSeconds =
                chapterSeconds -
                streamClip->time_in_sec +
                streamClip->relative_time_in_sec;
            
#ifdef DEBUG
            printf("streamFileIndex %2i: (%9i / %f = %8.3f) - %8.3f + %8.3f = %8.3f\n", streamFileIndex, chapterTime, TIMECODE_DIV, chapterSeconds, streamClip->time_in_sec, streamClip->relative_time_in_sec, relativeSeconds);
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
        
        *pos_ptr += CHAPTER_SIZE;
    }
    
    chapters = (double*) realloc(chapters, validChapterCount * sizeof(double));

    playlist.time_in_sec = mpls_file.time_in;
    playlist.time_out_sec = mpls_file.time_out;
    playlist.duration_sec = total_length_sec;
    playlist.stream_clips = streamClips;
    playlist.stream_clip_count = stream_clip_count;
    playlist.chapters = chapters;
    playlist.chapter_count = validChapterCount;
    
    char* playlist_duration_human = format_duration(playlist.duration_sec);

    printf("Playlist length: %s\n", playlist_duration_human);
    printf("Chapter count: %i\n", validChapterCount);
    
    free(playlist_duration_human);
    
    int c;
    for(c = 0; c < playlist.chapter_count; c++)
    {
        double sec = playlist.chapters[c];
        char* chapter_start_human = format_duration(sec);
        printf("Chapter %2i: %s\n", c + 1, chapter_start_human);
        free(chapter_start_human);
    }
    
    printf("\n");

    free(streamClips);
    free(chapterStreamClips);
    free_playlist_members(&playlist);
    free_mpls_file_members(&mpls_file);
}


/*
 * 
 */
int main(int argc, char** argv) {
    if (argc < 2)
    {
        DIE("Usage: parse_mpls MPLS_FILE_PATH [ MPLS_FILE_PATH ... ]");
    }
    
    int i;
    for(i = 1; i < argc; i++)
    {
        parse_mpls(argv[i]);
    }
    
    return (EXIT_SUCCESS);
}

