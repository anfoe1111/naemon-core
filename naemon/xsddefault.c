#include "config.h"
#include "common.h"
#include "defaults.h"
#include "statusdata.h"
#include "comments.h"
#include "downtime.h"
#include "macros.h"
#include "xsddefault.h"
#include "utils.h"
#include "logging.h"
#include "globals.h"
#include "nm_alloc.h"
#include <string.h>

time_t program_start;
int daemon_mode;
time_t last_log_rotation;
int enable_notifications;
int execute_service_checks;
int accept_passive_service_checks;
int execute_host_checks;
int accept_passive_host_checks;
int enable_event_handlers;
int obsess_over_services;
int obsess_over_hosts;
int check_service_freshness;
int check_host_freshness;
int enable_flap_detection;
int process_performance_data;
int nagios_pid;
int buffer_stats[1][3];
int program_stats[MAX_CHECK_STATS_TYPES][3];

/******************************************************************/
/********************* INIT/CLEANUP FUNCTIONS *********************/
/******************************************************************/

/* initialize status data */
int xsddefault_initialize_status_data(const char *cfgfile)
{
	nagios_macros *mac;

	/* initialize locations if necessary */
	if (!status_file)
		status_file = nm_strdup(get_default_status_file());

	/* make sure we have what we need */
	if (status_file == NULL)
		return ERROR;

	mac = get_global_macros();
	/* save the status file macro */
	my_free(mac->x[MACRO_STATUSDATAFILE]);
	mac->x[MACRO_STATUSDATAFILE] = nm_strdup(status_file);
	strip(mac->x[MACRO_STATUSDATAFILE]);

	/* delete the old status log (it might not exist) */
	if (status_file)
		unlink(status_file);

	return OK;
}


/* cleanup status data before terminating */
int xsddefault_cleanup_status_data(int delete_status_data)
{

	/* delete the status log */
	if (delete_status_data == TRUE && status_file) {
		if (unlink(status_file))
			return ERROR;
	}

	/* free memory */
	my_free(status_file);

	return OK;
}


/******************************************************************/
/****************** STATUS DATA OUTPUT FUNCTIONS ******************/
/******************************************************************/

/* write all status data to file */
int xsddefault_save_status_data(void)
{
	char *tmp_log = NULL;
	customvariablesmember *temp_customvariablesmember = NULL;
	host *temp_host = NULL;
	service *temp_service = NULL;
	contact *temp_contact = NULL;
	comment *temp_comment = NULL;
	scheduled_downtime *temp_downtime = NULL;
	time_t current_time;
	int fd = 0;
	FILE *fp = NULL;
	int result = OK;

	log_debug_info(DEBUGL_FUNCTIONS, 0, "save_status_data()\n");

	/* users may not want us to write status data */
	if (!status_file || !strcmp(status_file, "/dev/null"))
		return OK;

	nm_asprintf(&tmp_log, "%sXXXXXX", temp_file);
	if (tmp_log == NULL)
		return ERROR;

	log_debug_info(DEBUGL_STATUSDATA, 2, "Writing status data to temp file '%s'\n", tmp_log);

	if ((fd = mkstemp(tmp_log)) == -1) {

		/* log an error */
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Unable to create temp file '%s' for writing status data: %s\n", tmp_log, strerror(errno));

		/* free memory */
		my_free(tmp_log);

		return ERROR;
	}
	fp = (FILE *)fdopen(fd, "w");
	if (fp == NULL) {

		close(fd);
		unlink(tmp_log);

		/* log an error */
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Unable to open temp file '%s' for writing status data: %s\n", tmp_log, strerror(errno));

		/* free memory */
		my_free(tmp_log);

		return ERROR;
	}

	/* generate check statistics */
	generate_check_stats();

	/* write version info to status file */
	fprintf(fp, "########################################\n");
	fprintf(fp, "#          NAGIOS STATUS FILE\n");
	fprintf(fp, "#\n");
	fprintf(fp, "# THIS FILE IS AUTOMATICALLY GENERATED\n");
	fprintf(fp, "# BY NAGIOS.  DO NOT MODIFY THIS FILE!\n");
	fprintf(fp, "########################################\n\n");

	time(&current_time);

	/* write file info */
	fprintf(fp, "info {\n");
	fprintf(fp, "\tcreated=%lu\n", current_time);
	fprintf(fp, "\tversion=%s\n", PROGRAM_VERSION);
	fprintf(fp, "\t}\n\n");

	/* save program status data */
	fprintf(fp, "programstatus {\n");
	fprintf(fp, "\tmodified_host_attributes=%lu\n", modified_host_process_attributes);
	fprintf(fp, "\tmodified_service_attributes=%lu\n", modified_service_process_attributes);
	fprintf(fp, "\tnagios_pid=%d\n", nagios_pid);
	fprintf(fp, "\tdaemon_mode=%d\n", daemon_mode);
	fprintf(fp, "\tprogram_start=%lu\n", program_start);
	fprintf(fp, "\tlast_log_rotation=%lu\n", last_log_rotation);
	fprintf(fp, "\tenable_notifications=%d\n", enable_notifications);
	fprintf(fp, "\tactive_service_checks_enabled=%d\n", execute_service_checks);
	fprintf(fp, "\tpassive_service_checks_enabled=%d\n", accept_passive_service_checks);
	fprintf(fp, "\tactive_host_checks_enabled=%d\n", execute_host_checks);
	fprintf(fp, "\tpassive_host_checks_enabled=%d\n", accept_passive_host_checks);
	fprintf(fp, "\tenable_event_handlers=%d\n", enable_event_handlers);
	fprintf(fp, "\tobsess_over_services=%d\n", obsess_over_services);
	fprintf(fp, "\tobsess_over_hosts=%d\n", obsess_over_hosts);
	fprintf(fp, "\tcheck_service_freshness=%d\n", check_service_freshness);
	fprintf(fp, "\tcheck_host_freshness=%d\n", check_host_freshness);
	fprintf(fp, "\tenable_flap_detection=%d\n", enable_flap_detection);
	fprintf(fp, "\tprocess_performance_data=%d\n", process_performance_data);
	fprintf(fp, "\tglobal_host_event_handler=%s\n", (global_host_event_handler == NULL) ? "" : global_host_event_handler);
	fprintf(fp, "\tglobal_service_event_handler=%s\n", (global_service_event_handler == NULL) ? "" : global_service_event_handler);
	fprintf(fp, "\tnext_comment_id=%lu\n", next_comment_id);
	fprintf(fp, "\tnext_downtime_id=%lu\n", next_downtime_id);
	fprintf(fp, "\tnext_event_id=%lu\n", next_event_id);
	fprintf(fp, "\tnext_problem_id=%lu\n", next_problem_id);
	fprintf(fp, "\tnext_notification_id=%lu\n", next_notification_id);
	fprintf(fp, "\tactive_scheduled_host_check_stats=%d,%d,%d\n", check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[0], check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[1], check_statistics[ACTIVE_SCHEDULED_HOST_CHECK_STATS].minute_stats[2]);
	fprintf(fp, "\tactive_ondemand_host_check_stats=%d,%d,%d\n", check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[0], check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[1], check_statistics[ACTIVE_ONDEMAND_HOST_CHECK_STATS].minute_stats[2]);
	fprintf(fp, "\tpassive_host_check_stats=%d,%d,%d\n", check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[0], check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[1], check_statistics[PASSIVE_HOST_CHECK_STATS].minute_stats[2]);
	fprintf(fp, "\tactive_scheduled_service_check_stats=%d,%d,%d\n", check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[0], check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[1], check_statistics[ACTIVE_SCHEDULED_SERVICE_CHECK_STATS].minute_stats[2]);
	fprintf(fp, "\tactive_ondemand_service_check_stats=%d,%d,%d\n", check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[0], check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[1], check_statistics[ACTIVE_ONDEMAND_SERVICE_CHECK_STATS].minute_stats[2]);
	fprintf(fp, "\tpassive_service_check_stats=%d,%d,%d\n", check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[0], check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[1], check_statistics[PASSIVE_SERVICE_CHECK_STATS].minute_stats[2]);
	fprintf(fp, "\tcached_host_check_stats=%d,%d,%d\n", check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[0], check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[1], check_statistics[ACTIVE_CACHED_HOST_CHECK_STATS].minute_stats[2]);
	fprintf(fp, "\tcached_service_check_stats=%d,%d,%d\n", check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[0], check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[1], check_statistics[ACTIVE_CACHED_SERVICE_CHECK_STATS].minute_stats[2]);
	fprintf(fp, "\texternal_command_stats=%d,%d,%d\n", check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[0], check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[1], check_statistics[EXTERNAL_COMMAND_STATS].minute_stats[2]);

	fprintf(fp, "\tparallel_host_check_stats=%d,%d,%d\n", check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[0], check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[1], check_statistics[PARALLEL_HOST_CHECK_STATS].minute_stats[2]);
	fprintf(fp, "\tserial_host_check_stats=%d,%d,%d\n", check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[0], check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[1], check_statistics[SERIAL_HOST_CHECK_STATS].minute_stats[2]);
	fprintf(fp, "\t}\n\n");


	/* save host status data */
	for (temp_host = host_list; temp_host != NULL; temp_host = temp_host->next) {

		fprintf(fp, "hoststatus {\n");
		fprintf(fp, "\thost_name=%s\n", temp_host->name);

		fprintf(fp, "\tmodified_attributes=%lu\n", temp_host->modified_attributes);
		fprintf(fp, "\tcheck_command=%s\n", (temp_host->check_command == NULL) ? "" : temp_host->check_command);
		fprintf(fp, "\tcheck_period=%s\n", (temp_host->check_period == NULL) ? "" : temp_host->check_period);
		fprintf(fp, "\tnotification_period=%s\n", (temp_host->notification_period == NULL) ? "" : temp_host->notification_period);
		fprintf(fp, "\tcheck_interval=%f\n", temp_host->check_interval);
		fprintf(fp, "\tretry_interval=%f\n", temp_host->retry_interval);
		fprintf(fp, "\tevent_handler=%s\n", (temp_host->event_handler == NULL) ? "" : temp_host->event_handler);

		fprintf(fp, "\thas_been_checked=%d\n", temp_host->has_been_checked);
		fprintf(fp, "\tshould_be_scheduled=%d\n", temp_host->should_be_scheduled);
		fprintf(fp, "\tcheck_execution_time=%.3f\n", temp_host->execution_time);
		fprintf(fp, "\tcheck_latency=%.3f\n", temp_host->latency);
		fprintf(fp, "\tcheck_type=%d\n", temp_host->check_type);
		fprintf(fp, "\tcurrent_state=%d\n", temp_host->current_state);
		fprintf(fp, "\tlast_hard_state=%d\n", temp_host->last_hard_state);
		fprintf(fp, "\tlast_event_id=%lu\n", temp_host->last_event_id);
		fprintf(fp, "\tcurrent_event_id=%lu\n", temp_host->current_event_id);
		fprintf(fp, "\tcurrent_problem_id=%lu\n", temp_host->current_problem_id);
		fprintf(fp, "\tlast_problem_id=%lu\n", temp_host->last_problem_id);
		fprintf(fp, "\tplugin_output=%s\n", (temp_host->plugin_output == NULL) ? "" : temp_host->plugin_output);
		fprintf(fp, "\tlong_plugin_output=%s\n", (temp_host->long_plugin_output == NULL) ? "" : temp_host->long_plugin_output);
		fprintf(fp, "\tperformance_data=%s\n", (temp_host->perf_data == NULL) ? "" : temp_host->perf_data);
		fprintf(fp, "\tlast_check=%lu\n", temp_host->last_check);
		fprintf(fp, "\tnext_check=%lu\n", temp_host->next_check);
		fprintf(fp, "\tcheck_options=%d\n", temp_host->check_options);
		fprintf(fp, "\tcurrent_attempt=%d\n", temp_host->current_attempt);
		fprintf(fp, "\tmax_attempts=%d\n", temp_host->max_attempts);
		fprintf(fp, "\tstate_type=%d\n", temp_host->state_type);
		fprintf(fp, "\tlast_state_change=%lu\n", temp_host->last_state_change);
		fprintf(fp, "\tlast_hard_state_change=%lu\n", temp_host->last_hard_state_change);
		fprintf(fp, "\tlast_time_up=%lu\n", temp_host->last_time_up);
		fprintf(fp, "\tlast_time_down=%lu\n", temp_host->last_time_down);
		fprintf(fp, "\tlast_time_unreachable=%lu\n", temp_host->last_time_unreachable);
		fprintf(fp, "\tlast_notification=%lu\n", temp_host->last_notification);
		fprintf(fp, "\tnext_notification=%lu\n", temp_host->next_notification);
		fprintf(fp, "\tno_more_notifications=%d\n", temp_host->no_more_notifications);
		fprintf(fp, "\tcurrent_notification_number=%d\n", temp_host->current_notification_number);
		fprintf(fp, "\tcurrent_notification_id=%lu\n", temp_host->current_notification_id);
		fprintf(fp, "\tnotifications_enabled=%d\n", temp_host->notifications_enabled);
		fprintf(fp, "\tproblem_has_been_acknowledged=%d\n", temp_host->problem_has_been_acknowledged);
		fprintf(fp, "\tacknowledgement_type=%d\n", temp_host->acknowledgement_type);
		fprintf(fp, "\tactive_checks_enabled=%d\n", temp_host->checks_enabled);
		fprintf(fp, "\tpassive_checks_enabled=%d\n", temp_host->accept_passive_checks);
		fprintf(fp, "\tevent_handler_enabled=%d\n", temp_host->event_handler_enabled);
		fprintf(fp, "\tflap_detection_enabled=%d\n", temp_host->flap_detection_enabled);
		fprintf(fp, "\tprocess_performance_data=%d\n", temp_host->process_performance_data);
		fprintf(fp, "\tobsess=%d\n", temp_host->obsess);
		fprintf(fp, "\tlast_update=%lu\n", current_time);
		fprintf(fp, "\tis_flapping=%d\n", temp_host->is_flapping);
		fprintf(fp, "\tpercent_state_change=%.2f\n", temp_host->percent_state_change);
		fprintf(fp, "\tscheduled_downtime_depth=%d\n", temp_host->scheduled_downtime_depth);
		/* custom variables */
		for (temp_customvariablesmember = temp_host->custom_variables; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {
			if (temp_customvariablesmember->variable_name)
				fprintf(fp, "\t_%s=%d;%s\n", temp_customvariablesmember->variable_name, temp_customvariablesmember->has_been_modified, (temp_customvariablesmember->variable_value == NULL) ? "" : temp_customvariablesmember->variable_value);
		}
		fprintf(fp, "\t}\n\n");
	}

	/* save service status data */
	for (temp_service = service_list; temp_service != NULL; temp_service = temp_service->next) {

		fprintf(fp, "servicestatus {\n");
		fprintf(fp, "\thost_name=%s\n", temp_service->host_name);

		fprintf(fp, "\tservice_description=%s\n", temp_service->description);
		fprintf(fp, "\tmodified_attributes=%lu\n", temp_service->modified_attributes);
		fprintf(fp, "\tcheck_command=%s\n", (temp_service->check_command == NULL) ? "" : temp_service->check_command);
		fprintf(fp, "\tcheck_period=%s\n", (temp_service->check_period == NULL) ? "" : temp_service->check_period);
		fprintf(fp, "\tnotification_period=%s\n", (temp_service->notification_period == NULL) ? "" : temp_service->notification_period);
		fprintf(fp, "\tcheck_interval=%f\n", temp_service->check_interval);
		fprintf(fp, "\tretry_interval=%f\n", temp_service->retry_interval);
		fprintf(fp, "\tevent_handler=%s\n", (temp_service->event_handler == NULL) ? "" : temp_service->event_handler);

		fprintf(fp, "\thas_been_checked=%d\n", temp_service->has_been_checked);
		fprintf(fp, "\tshould_be_scheduled=%d\n", temp_service->should_be_scheduled);
		fprintf(fp, "\tcheck_execution_time=%.3f\n", temp_service->execution_time);
		fprintf(fp, "\tcheck_latency=%.3f\n", temp_service->latency);
		fprintf(fp, "\tcheck_type=%d\n", temp_service->check_type);
		fprintf(fp, "\tcurrent_state=%d\n", temp_service->current_state);
		fprintf(fp, "\tlast_hard_state=%d\n", temp_service->last_hard_state);
		fprintf(fp, "\tlast_event_id=%lu\n", temp_service->last_event_id);
		fprintf(fp, "\tcurrent_event_id=%lu\n", temp_service->current_event_id);
		fprintf(fp, "\tcurrent_problem_id=%lu\n", temp_service->current_problem_id);
		fprintf(fp, "\tlast_problem_id=%lu\n", temp_service->last_problem_id);
		fprintf(fp, "\tcurrent_attempt=%d\n", temp_service->current_attempt);
		fprintf(fp, "\tmax_attempts=%d\n", temp_service->max_attempts);
		fprintf(fp, "\tstate_type=%d\n", temp_service->state_type);
		fprintf(fp, "\tlast_state_change=%lu\n", temp_service->last_state_change);
		fprintf(fp, "\tlast_hard_state_change=%lu\n", temp_service->last_hard_state_change);
		fprintf(fp, "\tlast_time_ok=%lu\n", temp_service->last_time_ok);
		fprintf(fp, "\tlast_time_warning=%lu\n", temp_service->last_time_warning);
		fprintf(fp, "\tlast_time_unknown=%lu\n", temp_service->last_time_unknown);
		fprintf(fp, "\tlast_time_critical=%lu\n", temp_service->last_time_critical);
		fprintf(fp, "\tplugin_output=%s\n", (temp_service->plugin_output == NULL) ? "" : temp_service->plugin_output);
		fprintf(fp, "\tlong_plugin_output=%s\n", (temp_service->long_plugin_output == NULL) ? "" : temp_service->long_plugin_output);
		fprintf(fp, "\tperformance_data=%s\n", (temp_service->perf_data == NULL) ? "" : temp_service->perf_data);
		fprintf(fp, "\tlast_check=%lu\n", temp_service->last_check);
		fprintf(fp, "\tnext_check=%lu\n", temp_service->next_check);
		fprintf(fp, "\tcheck_options=%d\n", temp_service->check_options);
		fprintf(fp, "\tcurrent_notification_number=%d\n", temp_service->current_notification_number);
		fprintf(fp, "\tcurrent_notification_id=%lu\n", temp_service->current_notification_id);
		fprintf(fp, "\tlast_notification=%lu\n", temp_service->last_notification);
		fprintf(fp, "\tnext_notification=%lu\n", temp_service->next_notification);
		fprintf(fp, "\tno_more_notifications=%d\n", temp_service->no_more_notifications);
		fprintf(fp, "\tnotifications_enabled=%d\n", temp_service->notifications_enabled);
		fprintf(fp, "\tactive_checks_enabled=%d\n", temp_service->checks_enabled);
		fprintf(fp, "\tpassive_checks_enabled=%d\n", temp_service->accept_passive_checks);
		fprintf(fp, "\tevent_handler_enabled=%d\n", temp_service->event_handler_enabled);
		fprintf(fp, "\tproblem_has_been_acknowledged=%d\n", temp_service->problem_has_been_acknowledged);
		fprintf(fp, "\tacknowledgement_type=%d\n", temp_service->acknowledgement_type);
		fprintf(fp, "\tflap_detection_enabled=%d\n", temp_service->flap_detection_enabled);
		fprintf(fp, "\tprocess_performance_data=%d\n", temp_service->process_performance_data);
		fprintf(fp, "\tobsess=%d\n", temp_service->obsess);
		fprintf(fp, "\tlast_update=%lu\n", current_time);
		fprintf(fp, "\tis_flapping=%d\n", temp_service->is_flapping);
		fprintf(fp, "\tpercent_state_change=%.2f\n", temp_service->percent_state_change);
		fprintf(fp, "\tscheduled_downtime_depth=%d\n", temp_service->scheduled_downtime_depth);
		/* custom variables */
		for (temp_customvariablesmember = temp_service->custom_variables; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {
			if (temp_customvariablesmember->variable_name)
				fprintf(fp, "\t_%s=%d;%s\n", temp_customvariablesmember->variable_name, temp_customvariablesmember->has_been_modified, (temp_customvariablesmember->variable_value == NULL) ? "" : temp_customvariablesmember->variable_value);
		}
		fprintf(fp, "\t}\n\n");
	}

	/* save contact status data */
	for (temp_contact = contact_list; temp_contact != NULL; temp_contact = temp_contact->next) {

		fprintf(fp, "contactstatus {\n");
		fprintf(fp, "\tcontact_name=%s\n", temp_contact->name);

		fprintf(fp, "\tmodified_attributes=%lu\n", temp_contact->modified_attributes);
		fprintf(fp, "\tmodified_host_attributes=%lu\n", temp_contact->modified_host_attributes);
		fprintf(fp, "\tmodified_service_attributes=%lu\n", temp_contact->modified_service_attributes);
		fprintf(fp, "\thost_notification_period=%s\n", (temp_contact->host_notification_period == NULL) ? "" : temp_contact->host_notification_period);
		fprintf(fp, "\tservice_notification_period=%s\n", (temp_contact->service_notification_period == NULL) ? "" : temp_contact->service_notification_period);

		fprintf(fp, "\tlast_host_notification=%lu\n", temp_contact->last_host_notification);
		fprintf(fp, "\tlast_service_notification=%lu\n", temp_contact->last_service_notification);
		fprintf(fp, "\thost_notifications_enabled=%d\n", temp_contact->host_notifications_enabled);
		fprintf(fp, "\tservice_notifications_enabled=%d\n", temp_contact->service_notifications_enabled);
		/* custom variables */
		for (temp_customvariablesmember = temp_contact->custom_variables; temp_customvariablesmember != NULL; temp_customvariablesmember = temp_customvariablesmember->next) {
			if (temp_customvariablesmember->variable_name)
				fprintf(fp, "\t_%s=%d;%s\n", temp_customvariablesmember->variable_name, temp_customvariablesmember->has_been_modified, (temp_customvariablesmember->variable_value == NULL) ? "" : temp_customvariablesmember->variable_value);
		}
		fprintf(fp, "\t}\n\n");
	}

	/* save all comments */
	for (temp_comment = comment_list; temp_comment != NULL; temp_comment = temp_comment->next) {

		if (temp_comment->comment_type == HOST_COMMENT)
			fprintf(fp, "hostcomment {\n");
		else
			fprintf(fp, "servicecomment {\n");
		fprintf(fp, "\thost_name=%s\n", temp_comment->host_name);
		if (temp_comment->comment_type == SERVICE_COMMENT)
			fprintf(fp, "\tservice_description=%s\n", temp_comment->service_description);
		fprintf(fp, "\tentry_type=%d\n", temp_comment->entry_type);
		fprintf(fp, "\tcomment_id=%lu\n", temp_comment->comment_id);
		fprintf(fp, "\tsource=%d\n", temp_comment->source);
		fprintf(fp, "\tpersistent=%d\n", temp_comment->persistent);
		fprintf(fp, "\tentry_time=%lu\n", temp_comment->entry_time);
		fprintf(fp, "\texpires=%d\n", temp_comment->expires);
		fprintf(fp, "\texpire_time=%lu\n", temp_comment->expire_time);
		fprintf(fp, "\tauthor=%s\n", temp_comment->author);
		fprintf(fp, "\tcomment_data=%s\n", temp_comment->comment_data);
		fprintf(fp, "\t}\n\n");
	}

	/* save all downtime */
	for (temp_downtime = scheduled_downtime_list; temp_downtime != NULL; temp_downtime = temp_downtime->next) {

		if (temp_downtime->type == HOST_DOWNTIME)
			fprintf(fp, "hostdowntime {\n");
		else
			fprintf(fp, "servicedowntime {\n");
		fprintf(fp, "\thost_name=%s\n", temp_downtime->host_name);
		if (temp_downtime->type == SERVICE_DOWNTIME)
			fprintf(fp, "\tservice_description=%s\n", temp_downtime->service_description);
		fprintf(fp, "\tdowntime_id=%lu\n", temp_downtime->downtime_id);
		fprintf(fp, "\tcomment_id=%lu\n", temp_downtime->comment_id);
		fprintf(fp, "\tentry_time=%lu\n", temp_downtime->entry_time);
		fprintf(fp, "\tstart_time=%lu\n", temp_downtime->start_time);
		fprintf(fp, "\tflex_downtime_start=%lu\n", temp_downtime->flex_downtime_start);
		fprintf(fp, "\tend_time=%lu\n", temp_downtime->end_time);
		fprintf(fp, "\ttriggered_by=%lu\n", temp_downtime->triggered_by);
		fprintf(fp, "\tfixed=%d\n", temp_downtime->fixed);
		fprintf(fp, "\tduration=%lu\n", temp_downtime->duration);
		fprintf(fp, "\tis_in_effect=%d\n", temp_downtime->is_in_effect);
		fprintf(fp, "\tstart_notification_sent=%d\n", temp_downtime->start_notification_sent);
		fprintf(fp, "\tauthor=%s\n", temp_downtime->author);
		fprintf(fp, "\tcomment=%s\n", temp_downtime->comment);
		fprintf(fp, "\t}\n\n");
	}


	/* reset file permissions */
	fchmod(fd, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH);

	/* flush the file to disk */
	fflush(fp);

	/* fsync the file so that it is completely written out before moving it */
	fsync(fd);

	/* close the temp file */
	result = fclose(fp);

	/* save/close was successful */
	if (result == 0) {

		result = OK;

		/* move the temp file to the status log (overwrite the old status log) */
		if (my_rename(tmp_log, status_file)) {
			unlink(tmp_log);
			logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Unable to update status data file '%s': %s", status_file, strerror(errno));
			result = ERROR;
		}
	}

	/* a problem occurred saving the file */
	else {

		result = ERROR;

		/* remove temp file and log an error */
		unlink(tmp_log);
		logit(NSLOG_RUNTIME_ERROR, TRUE, "Error: Unable to save status file: %s", strerror(errno));
	}

	/* free memory */
	my_free(tmp_log);

	return result;
}
