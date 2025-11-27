
function f = make_filter(cycles, increment, atten)

    % Validate input arguments
    if ne(nargin, 3)
        error('Usage: make_filter(cycles, increment, atten)');
    end
    filename = sprintf("Sinc%d.h", cycles);
    % Generate and measure filters
    fudge_factor1 = 1.0;
    f1 = generate_filter(cycles, fudge_factor1, increment, atten);
    [stop_atten, stop_band_start1, minus_3db] = measure_filter(f1, atten);
    # printf("    fudge_factor : %15.13f   stop_band_start : %15.13f   1\n", fudge_factor1, stop_band_start1);

    fudge_factor2 = 1.25;
    f2 = generate_filter(cycles, fudge_factor2, increment, atten);
    [stop_atten, stop_band_start2, minus_3db] = measure_filter(f2, atten);
    # printf("    fudge_factor : %15.13f   stop_band_start : %15.13f   2\n", fudge_factor2, stop_band_start2);

    # Optimize filter based on stop_band_start
    f = f1;
    fudge_factor = fudge_factor1;
    stop_band_start = stop_band_start1;

    while (abs(stop_band_start1 - stop_band_start2) > 1e-10)
        if (stop_band_start1 < stop_band_start2)
            printf("stop_band_start1 < stop_band_start2\n");
            break;
        end

        fudge_factor = fudge_factor1 + (fudge_factor2 - fudge_factor1) / 2;
        f = generate_filter(cycles, fudge_factor, increment, atten);
        [stop_atten, stop_band_start, minus_3db] = measure_filter(f, atten);

        if (stop_band_start > 1.0)
            printf("A %10.8f   %10.8f   %10.8f\n", fudge_factor1, fudge_factor, fudge_factor2);
            continue;
        end

        if (stop_band_start < 0.5 / increment)
            f2 = f;
            stop_band_start2 = stop_band_start;
            fudge_factor2 = fudge_factor;
            choice = 2;
        else
            f1 = f;
            stop_band_start1 = stop_band_start;
            fudge_factor1 = fudge_factor;
            choice = 1;
        end

        # printf("    fudge_factor : %15.13f   stop_band_start : %15.13f   %d\n", fudge_factor, stop_band_start, choice);
    end

    printf("\n");

    % Grab only half the coefficients
    N = length(f);
    f = increment * f' ;

    if mod(N, 2) == 0
        index = find(f == max(f));

        if numel(index) == 2
            # Interpolate between the two maximum values (we expect an odd number symmetry)
            newF = f;
            for i = 1:(N-1)
                newF(i) = f(i) + (f(i+1) - f(i)) / 2;
            end
            f = newF; # Update f with the interpolated values
            index = find(f == max(f));
        else
            index = min(index);
        end
        half_f = f(index:end);
    else
        error("Length of the filter coefficients should be even.");
    end

    trailing_zeros = 4 - mod(length(half_f), 4);

    # Print analysis
    printf("# f = make_filter (%d, %d, %4.1f) ;\n", cycles, increment, atten);
    printf("#   Coeff. count     : %d\n", N);
    printf("#   Fudge factor     : %9.7f\n", fudge_factor);
    printf("#   Pass band width  : %12.10f (should be %12.10f)\n", stop_band_start, 0.5 / increment);
    printf("#   Stop band atten. : %5.2f dB\n", abs(stop_atten));
    printf("#   -3dB band Width  : %5.3f\n", 0.5 / increment / minus_3db);
    printf("#   Half length      : %d\n", length(half_f) + trailing_zeros);
    printf("#   Increment        : %d\n", increment);

    # Save everything to a file if filename is provided
    file = fopen(filename, 'w');
    if file == -1
        error('Error: Unable to open file %s for writing.', filename);
    end

    # Write FIR coefficients to temporary file
    file_tmp = fopen('tmp.txt', 'w');
    if file_tmp == -1
        error('Error: Unable to open temporary file for writing.');
    end
    # Write coefficients with 8 values per row
    coeff_count = length(half_f);
    for i = 1:coeff_count
        if mod(i-1, 8) == 0
            fprintf(file_tmp, "      ");
        end
        fprintf(file_tmp, "%9.6ef,", half_f(i));
        if mod(i, 8) == 0 || i == coeff_count
            fprintf(file_tmp, "\n");
        else
            fprintf(file_tmp, " ");
        end
    end
    # Add trailing zeros if needed
    if trailing_zeros > 0
        start_pos = mod(coeff_count, 8);
        if start_pos != 0
            # Continue on same line
            for val = 1:trailing_zeros
                fprintf(file_tmp, " %9.6ef,", 0.0);
                if mod(start_pos + val, 8) == 0
                    fprintf(file_tmp, "\n");
                    if (val < trailing_zeros)
                        fprintf(file_tmp, "      ");
                    end
                end
            end
            if mod(start_pos + trailing_zeros, 8) != 0
                fprintf(file_tmp, "\n");
            end
        else
            # Start new line
            for val = 1:trailing_zeros
                if mod(val-1, 8) == 0
                    fprintf(file_tmp, "      ");
                end
                fprintf(file_tmp, "%9.6ef,", 0.0);
                if mod(val, 8) == 0 || val == trailing_zeros
                    fprintf(file_tmp, "\n");
                else
                    fprintf(file_tmp, " ");
                end
            end
        end
    end
    fclose(file_tmp);
    # Read back temporary file content
    fTmp = fopen('tmp.txt', 'rt');
    if fTmp == -1
        error('Error: Unable to open temporary file for reading.');
    end
    fir_coeef = fread(fTmp, '*char');
    fclose(fTmp);
    # Load template and substitute values
    templateFile = 'SincFilter.class.template'; % Template file name
    fid = fopen(templateFile, 'rt');
    if fid == -1
        error('Error: Unable to open template file.');
    end
    templateContent = fread(fid, '*char')';
    fclose(fid);
    # Substitute all values to the template
    templateContent = strrep(templateContent, '/*N*/', num2str(increment));
    templateContent = strrep(templateContent, '/*cycles*/', strrep(num2str(cycles),".","p"));
    templateContent = strrep(templateContent, '/*attenuation*/', strrep(num2str(round(atten*10)/10),".","p"));
    templateContent = strrep(templateContent, '/*FIR_Coefficients*/', fir_coeef);
    printf("concat comments\n");

    # Format and concatenate comment section
    comment_section = sprintf([
        ' *  f = make_filter (%d, %d, %4.1f, "%s") ;\n' ...
        ' *  N                : %d\n' ...
        ' *  Pass band width  : %9.7f (should be %9.7f)\n' ...
        ' *  Stop band atten. : %5.2f dB\n' ...
        ' *  -3dB band width  : %5.3f\n' ...
        ' *  Half length      : %d\n' ...
        ' *  Increment        : %d\n' ...
        ' *  FIR multiplies   : %d\n'], ...
        cycles, increment, atten, filename, ...
        N, stop_band_start, 0.5 / increment, ...
        abs(stop_atten), ...
        0.5 / increment / minus_3db, ...
        length(half_f), increment, round(N/increment) - 1);

    # Substitute the comment section into the template
    templateContent = strrep(templateContent, '{{CommentSection}}', comment_section);

    % Write final content to file
    fprintf(file, "%s", templateContent);
    fclose(file);
    printf("File generated: %s\n\n", filename);

endfunction